// Modern TypeScript definitions for C Bridge API
// Clean sidebar-only API without legacy drawer support

export interface WindowSize {
  width: number;
  height: number;
}

export interface SidebarState {
  visible: boolean;
}

export interface AppConfig {
  app: {
    name: string;
    version: string;
  };
  window: {
    title: string;
    width: number;
    height: number;
    resizable: boolean;
  };
  development: {
    debug_mode: boolean;
  };
}

// Internal message types
export interface BridgeMessage {
  id: number;
  method: string;
  params?: unknown;
}

export interface BridgeCallback {
  resolve: (value: unknown) => void;
  reject: (error: Error) => void;
}

export interface BridgeAPI {
  // Window functions
  window: {
    setSize(size: WindowSize): Promise<void>;
    getSize(): Promise<WindowSize>;
    minimize(): Promise<void>;
    maximize(): Promise<void>;
    restore(): Promise<void>;
  };

  // Modern Sidebar functions (NSSplitViewController)
  sidebar: {
    toggle(): Promise<void>;
    show(): Promise<void>;
    hide(): Promise<void>;
    getState(): Promise<SidebarState>;
  };

  // System functions
  system: {
    getPlatform(): Promise<"macos" | "windows" | "linux">;
    getConfig(): Promise<AppConfig>;
  };

  // Counter functions
  counter: {
    increment(): Promise<number>;
    decrement(): Promise<number>;
    getValue(): Promise<number>;
    setValue(args: { value: number }): Promise<number>;
  };

  // Demo functions
  demo: {
    greet(args: { name?: string }): Promise<string>;
  };
}

// Default export type for the bridge instance
declare const bridge: BridgeAPI;
export default bridge;

// Global augmentations
declare global {
  interface Window {
    bridge: BridgeAPI;
    handleBridgeResponse(id: number, success: boolean, result: unknown): void;
    // Native sidebar state callback for real-time updates
    nativeSidebar?: {
      onStateChange(visible: boolean): void;
    };
    webkit?: {
      messageHandlers: {
        bridge: {
          postMessage(message: string): void;
        };
      };
    };
  }
}

export {};
