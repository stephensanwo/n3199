// TCP Streaming Client for real-time data
export interface StreamMessage {
  type: string;
  stream: string;
  data: unknown;
  timestamp: number;
}

export interface StreamingClientOptions {
  host: string;
  port: number;
  reconnectInterval: number;
  maxReconnectAttempts: number;
}

export class StreamingClient {
  private socket: WebSocket | null = null;
  private isConnected = false;
  private reconnectAttempts = 0;
  private reconnectTimer: number | null = null;
  private messageHandlers = new Map<
    string,
    Set<(data: StreamMessage) => void>
  >();
  private options: StreamingClientOptions;

  constructor(options: Partial<StreamingClientOptions> = {}) {
    this.options = {
      host: "localhost",
      port: 8080,
      reconnectInterval: 3000,
      maxReconnectAttempts: 10,
      ...options,
    };
  }

  connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      try {
        // Use WebSocket to connect to the TCP server
        // Note: This requires a WebSocket-to-TCP bridge or the backend to support WebSocket protocol
        const url = `ws://${this.options.host}:${this.options.port}`;
        console.log(`[StreamingClient] Connecting to ${url}`);

        this.socket = new WebSocket(url);

        this.socket.onopen = () => {
          console.log("[StreamingClient] Connected to streaming server");
          this.isConnected = true;
          this.reconnectAttempts = 0;
          this.clearReconnectTimer();
          resolve();
        };

        this.socket.onmessage = (event) => {
          try {
            const message: StreamMessage = JSON.parse(event.data);
            console.log("[StreamingClient] Received message:", message);
            this.handleMessage(message);
          } catch (error) {
            console.error("[StreamingClient] Error parsing message:", error);
          }
        };

        this.socket.onclose = (event) => {
          console.log(
            "[StreamingClient] Connection closed:",
            event.code,
            event.reason
          );
          this.isConnected = false;
          this.socket = null;

          if (this.reconnectAttempts < this.options.maxReconnectAttempts) {
            this.scheduleReconnect();
          } else {
            console.error("[StreamingClient] Max reconnect attempts reached");
          }
        };

        this.socket.onerror = (error) => {
          console.error("[StreamingClient] WebSocket error:", error);
          if (!this.isConnected) {
            reject(new Error("Failed to connect to streaming server"));
          }
        };

        // Connection timeout
        setTimeout(() => {
          if (!this.isConnected) {
            this.socket?.close();
            reject(new Error("Connection timeout"));
          }
        }, 5000);
      } catch (error) {
        reject(error);
      }
    });
  }

  disconnect(): void {
    console.log("[StreamingClient] Disconnecting...");
    this.clearReconnectTimer();
    if (this.socket) {
      this.socket.close();
      this.socket = null;
    }
    this.isConnected = false;
  }

  private scheduleReconnect(): void {
    if (this.reconnectTimer) return;

    this.reconnectAttempts++;
    console.log(
      `[StreamingClient] Scheduling reconnect attempt ${this.reconnectAttempts}/${this.options.maxReconnectAttempts}`
    );

    this.reconnectTimer = window.setTimeout(() => {
      this.reconnectTimer = null;
      this.connect().catch((error) => {
        console.error("[StreamingClient] Reconnect failed:", error);
      });
    }, this.options.reconnectInterval);
  }

  private clearReconnectTimer(): void {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
  }

  private handleMessage(message: StreamMessage): void {
    const handlers = this.messageHandlers.get(message.stream);
    if (handlers) {
      handlers.forEach((handler) => {
        try {
          handler(message);
        } catch (error) {
          console.error("[StreamingClient] Error in message handler:", error);
        }
      });
    }
  }

  // Subscribe to stream data
  subscribe(
    streamName: string,
    handler: (data: StreamMessage) => void
  ): () => void {
    if (!this.messageHandlers.has(streamName)) {
      this.messageHandlers.set(streamName, new Set());
    }

    this.messageHandlers.get(streamName)!.add(handler);
    console.log(`[StreamingClient] Subscribed to stream: ${streamName}`);

    // Return unsubscribe function
    return () => {
      const handlers = this.messageHandlers.get(streamName);
      if (handlers) {
        handlers.delete(handler);
        if (handlers.size === 0) {
          this.messageHandlers.delete(streamName);
        }
      }
      console.log(`[StreamingClient] Unsubscribed from stream: ${streamName}`);
    };
  }

  // Send message to server (for stream control)
  send(message: Record<string, unknown>): void {
    if (this.socket && this.isConnected) {
      this.socket.send(JSON.stringify(message));
    } else {
      console.warn("[StreamingClient] Cannot send message - not connected");
    }
  }

  // Start a stream
  startStream(streamName: string): void {
    this.send({
      type: "start_stream",
      stream: streamName,
      timestamp: Date.now(),
    });
  }

  // Stop a stream
  stopStream(streamName: string): void {
    this.send({
      type: "stop_stream",
      stream: streamName,
      timestamp: Date.now(),
    });
  }

  // Get connection status
  getConnectionStatus(): { connected: boolean; reconnectAttempts: number } {
    return {
      connected: this.isConnected,
      reconnectAttempts: this.reconnectAttempts,
    };
  }
}

// Create singleton instance
export const streamingClient = new StreamingClient();
