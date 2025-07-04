import { useState, useEffect, useRef } from "react";
import bridge from "../bridge/bridge";
import {
  streamManager,
  STREAM_ENDPOINTS,
  type MemoryData,
} from "../bridge/stream";

export function StreamingData() {
  const [memoryData, setMemoryData] = useState<MemoryData | null>(null);
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState<string>("");
  const [streamingConfig, setStreamingConfig] = useState<{
    enabled: boolean;
    port: number;
  } | null>(null);
  const connectionInitialized = useRef(false);

  // Get streaming configuration
  useEffect(() => {
    const getStreamingConfig = async () => {
      try {
        const config = await bridge.streaming.getConfig();
        setStreamingConfig(config);
        console.log("Streaming config:", config);
      } catch (err) {
        console.error("Failed to get streaming config:", err);
        setError(`Failed to get streaming config: ${err}`);
      }
    };

    getStreamingConfig();
  }, []);

  // Setup stream manager and connect to memory stream
  useEffect(() => {
    if (!streamingConfig?.enabled || connectionInitialized.current) {
      return;
    }

    connectionInitialized.current = true;

    // Setup event handlers
    streamManager.onData<MemoryData>((data) => {
      console.log("Memory data received:", data);
      setMemoryData(data);
      setError(""); // Clear any previous errors
    });

    streamManager.onError((errorMsg) => {
      console.error("Stream error:", errorMsg);
      setError(errorMsg);
    });

    streamManager.onConnection((connected) => {
      console.log("Connection status changed:", connected);
      setIsConnected(connected);
      if (!connected) {
        setError("Stream disconnected");
      }
    });

    // Connect to memory stream
    const connectToStream = async () => {
      try {
        console.log("Connecting to memory stream...");
        await streamManager.connect(STREAM_ENDPOINTS.MEMORY);
      } catch (err) {
        console.error("Failed to connect to memory stream:", err);
        setError(`Failed to connect to memory stream: ${err}`);
      }
    };

    connectToStream();

    // Cleanup on unmount
    return () => {
      streamManager.disconnect();
      connectionInitialized.current = false;
    };
  }, [streamingConfig]);

  if (!streamingConfig) {
    return (
      <div className="card">
        <h3>📊 System Memory Monitor</h3>
        <p>Loading streaming configuration...</p>
      </div>
    );
  }

  if (!streamingConfig.enabled) {
    return (
      <div className="card">
        <h3>📊 System Memory Monitor</h3>
        <p style={{ color: "#666" }}>Streaming is disabled in configuration</p>
      </div>
    );
  }

  return (
    <div className="card">
      <h3>📊 System Memory Monitor</h3>

      {/* Connection Status */}
      <div style={{ marginBottom: "1rem" }}>
        <span
          style={{
            display: "inline-block",
            width: "10px",
            height: "10px",
            borderRadius: "50%",
            backgroundColor: isConnected ? "#4CAF50" : "#f44336",
            marginRight: "0.5rem",
          }}
        />
        <span style={{ fontSize: "0.9rem", color: "#666" }}>
          {isConnected ? "Connected" : "Disconnected"}
        </span>
      </div>

      {/* Error Display */}
      {error && (
        <div
          style={{
            color: "red",
            marginBottom: "1rem",
            padding: "0.5rem",
            border: "1px solid red",
            borderRadius: "4px",
            fontSize: "0.9rem",
          }}
        >
          {error}
        </div>
      )}

      {/* Memory Data */}
      {memoryData && !memoryData.error ? (
        <div>
          <div
            style={{ marginBottom: "1rem", fontSize: "0.9rem", color: "#666" }}
          >
            Last updated: {streamManager.formatTimestamp(memoryData.timestamp)}
          </div>

          {/* Memory Usage Bar */}
          <div style={{ marginBottom: "1rem" }}>
            <div
              style={{
                display: "flex",
                justifyContent: "space-between",
                marginBottom: "0.5rem",
              }}
            >
              <span>Memory Usage</span>
              <span>
                {streamManager.formatMemorySize(memoryData.used_mb)} /{" "}
                {streamManager.formatMemorySize(memoryData.total_mb)}
              </span>
            </div>
            <div
              style={{
                width: "100%",
                height: "20px",
                backgroundColor: "#e0e0e0",
                borderRadius: "10px",
                overflow: "hidden",
              }}
            >
              <div
                style={{
                  width: `${streamManager.calculateUsagePercentage(
                    memoryData.used_mb,
                    memoryData.total_mb
                  )}%`,
                  height: "100%",
                  backgroundColor: "#4CAF50",
                  transition: "width 0.3s ease",
                }}
              />
            </div>
            <div
              style={{
                fontSize: "0.8rem",
                color: "#666",
                marginTop: "0.25rem",
              }}
            >
              {streamManager
                .calculateUsagePercentage(
                  memoryData.used_mb,
                  memoryData.total_mb
                )
                .toFixed(1)}
              % used
            </div>
          </div>

          {/* Memory Breakdown */}
          <div
            style={{
              display: "grid",
              gridTemplateColumns: "1fr 1fr",
              gap: "0.5rem",
              fontSize: "0.9rem",
            }}
          >
            <div>
              <strong>Active:</strong>{" "}
              {streamManager.formatMemorySize(memoryData.active_mb)}
            </div>
            <div>
              <strong>Inactive:</strong>{" "}
              {streamManager.formatMemorySize(memoryData.inactive_mb)}
            </div>
            <div>
              <strong>Wired:</strong>{" "}
              {streamManager.formatMemorySize(memoryData.wired_mb)}
            </div>
            {memoryData.compressed_mb !== undefined && (
              <div>
                <strong>Compressed:</strong>{" "}
                {streamManager.formatMemorySize(memoryData.compressed_mb)}
              </div>
            )}
            <div>
              <strong>Free:</strong>{" "}
              {streamManager.formatMemorySize(memoryData.free_mb)}
            </div>
          </div>

          {/* Debug Info */}
          <div
            style={{
              marginTop: "1rem",
              fontSize: "0.8rem",
              color: "#888",
              padding: "0.5rem",
              backgroundColor: "#f9f9f9",
              borderRadius: "4px",
            }}
          >
            <strong>Debug:</strong> Data received and parsed successfully
          </div>
        </div>
      ) : memoryData?.error ? (
        <div style={{ color: "#f44336", fontSize: "0.9rem" }}>
          Error: {memoryData.error}
        </div>
      ) : (
        <div>
          <div style={{ color: "#666", fontSize: "0.9rem" }}>
            Waiting for memory data...
          </div>
          {/* Debug Info */}
          <div
            style={{
              marginTop: "1rem",
              fontSize: "0.8rem",
              color: "#888",
              padding: "0.5rem",
              backgroundColor: "#f9f9f9",
              borderRadius: "4px",
            }}
          >
            <strong>Debug:</strong> Connected: {isConnected ? "Yes" : "No"} |
            Data: {memoryData ? "Received" : "None"}
          </div>
        </div>
      )}
    </div>
  );
}

export default StreamingData;
