// Stream implementation for frontend
import type {
  StreamData,
  StreamConnection,
  StreamEventHandler,
  StreamErrorHandler,
  StreamConnectionHandler,
  StreamManager,
  StreamEndpointType,
} from "./stream.d";
import bridge from "./bridge";

// Available stream endpoints
export const STREAM_ENDPOINTS = {
  MEMORY: "/stream/memory",
  TCPDUMP: "/stream/tcpdump",
} as const;

// Re-export all types for convenience
export type {
  StreamData,
  MemoryData,
  TcpDumpData,
  StreamConnection,
  StreamEventHandler,
  StreamErrorHandler,
  StreamConnectionHandler,
  StreamManager,
  StreamEndpointType,
} from "./stream.d";

class StreamManagerImpl implements StreamManager {
  private connection: StreamConnection = {
    url: "",
    eventSource: null,
    isConnected: false,
    error: null,
    lastData: null,
  };

  private dataHandlers: StreamEventHandler[] = [];
  private errorHandlers: StreamErrorHandler[] = [];
  private connectionHandlers: StreamConnectionHandler[] = [];

  async connect(endpoint: StreamEndpointType): Promise<void> {
    try {
      // Get server URL from bridge
      const serverUrl = await bridge.streaming.getServerUrl();
      const streamUrl = `${serverUrl}${endpoint}`;

      console.log(`[StreamManager] Connecting to: ${streamUrl}`);

      // Close existing connection
      this.disconnect();

      // Create new EventSource
      const eventSource = new EventSource(streamUrl);
      this.connection.url = streamUrl;
      this.connection.eventSource = eventSource;
      this.connection.error = null;

      // Setup event handlers
      eventSource.onopen = () => {
        console.log(`[StreamManager] Connected to ${streamUrl}`);
        this.connection.isConnected = true;
        this.notifyConnectionHandlers(true);
      };

      eventSource.onmessage = (event) => {
        console.log(`[StreamManager] Message received:`, event.data);
        this.handleData(event.data);
      };

      // Listen for 'data' events specifically (server sends events with this type)
      eventSource.addEventListener("data", (event) => {
        console.log(`[StreamManager] Data event received:`, event.data);
        this.handleData(event.data);
      });

      eventSource.onerror = (event) => {
        console.error(`[StreamManager] Stream error:`, event);
        this.connection.isConnected = false;
        const errorMsg = `Stream connection error for ${endpoint}`;
        this.connection.error = errorMsg;
        this.notifyErrorHandlers(errorMsg);
        this.notifyConnectionHandlers(false);
      };
    } catch (error) {
      const errorMsg = `Failed to connect to stream: ${error}`;
      console.error(`[StreamManager] ${errorMsg}`);
      this.connection.error = errorMsg;
      this.notifyErrorHandlers(errorMsg);
      throw new Error(errorMsg);
    }
  }

  disconnect(): void {
    if (this.connection.eventSource) {
      console.log(`[StreamManager] Disconnecting from ${this.connection.url}`);
      this.connection.eventSource.close();
      this.connection.eventSource = null;
    }

    this.connection.isConnected = false;
    this.connection.url = "";
    this.connection.error = null;
    this.notifyConnectionHandlers(false);
  }

  isConnected(): boolean {
    return this.connection.isConnected;
  }

  onData<T extends StreamData = StreamData>(
    handler: StreamEventHandler<T>
  ): void {
    this.dataHandlers.push(handler as StreamEventHandler);
  }

  onError(handler: StreamErrorHandler): void {
    this.errorHandlers.push(handler);
  }

  onConnection(handler: StreamConnectionHandler): void {
    this.connectionHandlers.push(handler);
  }

  getLastData<T extends StreamData = StreamData>(): T | null {
    return this.connection.lastData as T | null;
  }

  getConnectionInfo(): StreamConnection {
    return { ...this.connection };
  }

  formatTimestamp(timestamp: number): string {
    return new Date(timestamp * 1000).toLocaleTimeString();
  }

  formatMemorySize(mb: number): string {
    if (mb >= 1024) {
      return `${(mb / 1024).toFixed(1)} GB`;
    }
    return `${mb.toFixed(0)} MB`;
  }

  calculateUsagePercentage(used: number, total: number): number {
    return total > 0 ? (used / total) * 100 : 0;
  }

  // Private methods
  private handleData(rawData: string): void {
    try {
      const data = JSON.parse(rawData) as StreamData;
      this.connection.lastData = data;
      this.notifyDataHandlers(data);
    } catch (error) {
      const errorMsg = `Failed to parse stream data: ${error}`;
      console.error(`[StreamManager] ${errorMsg}`);
      this.connection.error = errorMsg;
      this.notifyErrorHandlers(errorMsg);
    }
  }

  private notifyDataHandlers(data: StreamData): void {
    this.dataHandlers.forEach((handler) => {
      try {
        handler(data);
      } catch (error) {
        console.error(`[StreamManager] Error in data handler:`, error);
      }
    });
  }

  private notifyErrorHandlers(error: string): void {
    this.errorHandlers.forEach((handler) => {
      try {
        handler(error);
      } catch (err) {
        console.error(`[StreamManager] Error in error handler:`, err);
      }
    });
  }

  private notifyConnectionHandlers(connected: boolean): void {
    this.connectionHandlers.forEach((handler) => {
      try {
        handler(connected);
      } catch (error) {
        console.error(`[StreamManager] Error in connection handler:`, error);
      }
    });
  }
}

// Create and export stream manager instance
export const streamManager = new StreamManagerImpl();

// Utility function to create a new stream manager instance
export function createStreamManager(): StreamManager {
  return new StreamManagerImpl();
}

// Helper functions for common stream operations
export async function connectToMemoryStream(): Promise<StreamManager> {
  const manager = createStreamManager();
  await manager.connect(STREAM_ENDPOINTS.MEMORY);
  return manager;
}

export async function connectToTcpDumpStream(): Promise<StreamManager> {
  const manager = createStreamManager();
  await manager.connect(STREAM_ENDPOINTS.TCPDUMP);
  return manager;
}

// Export default stream manager
export default streamManager;
