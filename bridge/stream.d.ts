// TypeScript definitions for Streaming System

// Stream data types
export interface StreamData {
  timestamp: number;
  stream: string;
  [key: string]: unknown;
}

export interface MemoryData extends StreamData {
  total_mb: number;
  used_mb: number;
  free_mb: number;
  active_mb: number;
  inactive_mb: number;
  wired_mb: number;
  compressed_mb?: number;
  error?: string;
}

export interface NetworkPacket {
  protocol: string;
  src: string;
  dst: string;
  size: number;
  time: number;
}

export interface TcpDumpData extends StreamData {
  packet_count: number;
  recent_packets: NetworkPacket[];
}

// Stream configuration types
export interface StreamEndpoint {
  name: string;
  endpoint: string;
  handler: string;
  interval_ms: number;
  enabled: boolean;
  description: string;
}

export interface StreamingConfig {
  enabled: boolean;
  server: {
    host: string;
    port: number;
    max_connections: number;
  };
  streams: StreamEndpoint[];
  stream_count: number;
}

// Stream connection types
export interface StreamConnection {
  url: string;
  eventSource: EventSource | null;
  isConnected: boolean;
  error: string | null;
  lastData: StreamData | null;
}

// Stream event handlers
export type StreamEventHandler<T extends StreamData = StreamData> = (
  data: T
) => void;
export type StreamErrorHandler = (error: string) => void;
export type StreamConnectionHandler = (connected: boolean) => void;

// Stream manager interface
export interface StreamManager {
  // Connection management
  connect(endpoint: string): Promise<void>;
  disconnect(): void;
  isConnected(): boolean;

  // Event handlers
  onData<T extends StreamData = StreamData>(
    handler: StreamEventHandler<T>
  ): void;
  onError(handler: StreamErrorHandler): void;
  onConnection(handler: StreamConnectionHandler): void;

  // Stream specific methods
  getLastData<T extends StreamData = StreamData>(): T | null;
  getConnectionInfo(): StreamConnection;

  // Utility methods
  formatTimestamp(timestamp: number): string;
  formatMemorySize(mb: number): string;
  calculateUsagePercentage(used: number, total: number): number;
}

// Available stream endpoints
export declare const STREAM_ENDPOINTS: {
  readonly MEMORY: "/stream/memory";
  readonly TCPDUMP: "/stream/tcpdump";
};

export type StreamEndpointType =
  (typeof STREAM_ENDPOINTS)[keyof typeof STREAM_ENDPOINTS];
