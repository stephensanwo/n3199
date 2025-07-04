import { useState, useEffect, useRef } from "react";
import bridge from "../bridge/bridge";
import {
  createStreamManager,
  STREAM_ENDPOINTS,
  type TcpDumpData,
  type StreamManager,
} from "../bridge/stream";

export function TcpDumpStream() {
  const [tcpData, setTcpData] = useState<TcpDumpData | null>(null);
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState<string>("");
  const [streamingConfig, setStreamingConfig] = useState<{
    enabled: boolean;
    port: number;
  } | null>(null);
  const streamManagerRef = useRef<StreamManager | null>(null);
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

  // Setup stream manager and connect to TCP dump stream
  useEffect(() => {
    if (!streamingConfig?.enabled || connectionInitialized.current) {
      return;
    }

    connectionInitialized.current = true;

    // Create a dedicated stream manager for this component
    const manager = createStreamManager();
    streamManagerRef.current = manager;

    // Setup event handlers
    manager.onData<TcpDumpData>((data) => {
      console.log("TCP dump data received:", data);
      setTcpData(data);
      setError(""); // Clear any previous errors
    });

    manager.onError((errorMsg) => {
      console.error("TCP dump stream error:", errorMsg);
      setError(errorMsg);
    });

    manager.onConnection((connected) => {
      console.log("TCP dump connection status changed:", connected);
      setIsConnected(connected);
      if (!connected) {
        setError("TCP dump stream disconnected");
      }
    });

    // Connect to TCP dump stream
    const connectToStream = async () => {
      try {
        console.log("Connecting to TCP dump stream...");
        await manager.connect(STREAM_ENDPOINTS.TCPDUMP);
      } catch (err) {
        console.error("Failed to connect to TCP dump stream:", err);
        setError(`Failed to connect to TCP dump stream: ${err}`);
      }
    };

    connectToStream();

    // Cleanup on unmount
    return () => {
      if (streamManagerRef.current) {
        streamManagerRef.current.disconnect();
        streamManagerRef.current = null;
      }
      connectionInitialized.current = false;
    };
  }, [streamingConfig]);

  if (!streamingConfig) {
    return (
      <div className="card">
        <h3>🌐 Network TCP Dump Monitor</h3>
        <p>Loading streaming configuration...</p>
      </div>
    );
  }

  if (!streamingConfig.enabled) {
    return (
      <div className="card">
        <h3>🌐 Network TCP Dump Monitor</h3>
        <p style={{ color: "#666" }}>Streaming is disabled in configuration</p>
      </div>
    );
  }

  return (
    <div className="card">
      <h3>🌐 Network TCP Dump Monitor</h3>

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

      {/* TCP Dump Data */}
      {tcpData && streamManagerRef.current ? (
        <div>
          <div
            style={{ marginBottom: "1rem", fontSize: "0.9rem", color: "#666" }}
          >
            Last updated:{" "}
            {streamManagerRef.current.formatTimestamp(tcpData.timestamp)}
          </div>

          {/* Packet Summary */}
          <div style={{ marginBottom: "1rem" }}>
            <div
              style={{
                display: "flex",
                justifyContent: "space-between",
                marginBottom: "0.5rem",
                fontWeight: "bold",
              }}
            >
              <span>Total Packets:</span>
              <span style={{ color: "#4CAF50" }}>{tcpData.packet_count}</span>
            </div>
            <div
              style={{
                display: "flex",
                justifyContent: "space-between",
                fontSize: "0.9rem",
                color: "#666",
              }}
            >
              <span>Recent Packets:</span>
              <span>{tcpData.recent_packets.length}</span>
            </div>
          </div>

          {/* Recent Packets */}
          <div style={{ marginBottom: "1rem" }}>
            <h4 style={{ marginBottom: "0.5rem", fontSize: "1rem" }}>
              Recent Packets:
            </h4>
            <div
              style={{
                maxHeight: "300px",
                overflowY: "auto",
                border: "1px solid #e0e0e0",
                borderRadius: "4px",
                padding: "0.5rem",
                backgroundColor: "#f9f9f9",
              }}
            >
              {tcpData.recent_packets.length > 0 ? (
                tcpData.recent_packets.map((packet, index) => (
                  <div
                    key={index}
                    style={{
                      marginBottom: "0.5rem",
                      padding: "0.5rem",
                      backgroundColor: "white",
                      borderRadius: "4px",
                      fontSize: "0.8rem",
                      fontFamily: "monospace",
                    }}
                  >
                    <div
                      style={{
                        display: "grid",
                        gridTemplateColumns: "1fr 2fr 1fr",
                        gap: "0.5rem",
                        marginBottom: "0.25rem",
                      }}
                    >
                      <span style={{ fontWeight: "bold", color: "#2196F3" }}>
                        {packet.protocol}
                      </span>
                      <span style={{ color: "#666" }}>
                        {packet.src} → {packet.dst}
                      </span>
                      <span style={{ color: "#4CAF50" }}>
                        {packet.size} bytes
                      </span>
                    </div>
                    <div style={{ fontSize: "0.7rem", color: "#888" }}>
                      {streamManagerRef.current?.formatTimestamp(packet.time)}
                    </div>
                  </div>
                ))
              ) : (
                <div
                  style={{
                    color: "#666",
                    textAlign: "center",
                    padding: "1rem",
                  }}
                >
                  No recent packets
                </div>
              )}
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
            <strong>Debug:</strong> TCP dump data received and parsed
            successfully
          </div>
        </div>
      ) : (
        <div>
          <div style={{ color: "#666", fontSize: "0.9rem" }}>
            Waiting for TCP dump data...
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
            Data: {tcpData ? "Received" : "None"}
          </div>
        </div>
      )}
    </div>
  );
}

export default TcpDumpStream;
