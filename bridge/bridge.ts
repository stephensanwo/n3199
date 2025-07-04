// Modern Bridge implementation for frontend
import type {
  BridgeAPI,
  BridgeMessage,
  BridgeCallback,
  WindowSize,
  AppConfig,
  NativeEventHandler,
} from "./bridge.d";

// Re-export all types for convenience
export type {
  BridgeAPI,
  BridgeMessage,
  BridgeCallback,
  WindowSize,
  AppConfig,
  NativeEventHandler,
} from "./bridge.d";

class Bridge implements BridgeAPI {
  private nextCallbackId = 1;
  private callbacks = new Map<number, BridgeCallback>();
  // NEW: Event listeners for native events
  private eventListeners = new Map<string, Set<NativeEventHandler>>();

  constructor() {
    // Handle responses from native layer
    window.handleBridgeResponse = (
      id: number,
      success: boolean,
      result: unknown
    ) => {
      const callback = this.callbacks.get(id);
      if (callback) {
        if (success) {
          callback.resolve(result);
        } else {
          callback.reject(new Error(String(result)));
        }
        this.callbacks.delete(id);
      }
    };
  }

  private async call<T>(method: string, params?: unknown): Promise<T> {
    const webkit = window.webkit;
    if (!webkit?.messageHandlers.bridge) {
      throw new Error(
        "Bridge not initialized: WebKit message handlers not available"
      );
    }

    return new Promise<T>((resolve, reject) => {
      const id = this.nextCallbackId++;

      // Store callback with proper typing
      this.callbacks.set(id, {
        resolve: (value: unknown) => resolve(value as T),
        reject,
      });

      // Create bridge message
      const message: BridgeMessage = {
        id,
        method,
        params: params || null,
      };

      // Send to native layer
      webkit.messageHandlers.bridge.postMessage(JSON.stringify(message));
    });
  }

  // NEW: Handle native events (called by native code)
  onNativeEvent(eventName: string, data?: unknown): void {
    console.log(`[Bridge] Native event received: ${eventName}`, data);

    const listeners = this.eventListeners.get(eventName);
    if (listeners) {
      listeners.forEach((handler) => {
        try {
          handler(data);
        } catch (error) {
          console.error(
            `[Bridge] Error handling native event '${eventName}':`,
            error
          );
        }
      });
    } else {
      console.warn(
        `[Bridge] No listeners registered for native event: ${eventName}`
      );
    }
  }

  // NEW: Add event listener for native events
  addEventListener(eventName: string, handler: NativeEventHandler): void {
    if (!this.eventListeners.has(eventName)) {
      this.eventListeners.set(eventName, new Set());
    }
    this.eventListeners.get(eventName)!.add(handler);
    console.log(`[Bridge] Event listener added for: ${eventName}`);
  }

  // NEW: Remove event listener
  removeEventListener(eventName: string, handler: NativeEventHandler): void {
    const listeners = this.eventListeners.get(eventName);
    if (listeners) {
      listeners.delete(handler);
      if (listeners.size === 0) {
        this.eventListeners.delete(eventName);
      }
      console.log(`[Bridge] Event listener removed for: ${eventName}`);
    }
  }

  // Window functions
  window = {
    setSize: (size: WindowSize) => this.call<void>("window.setSize", size),
    getSize: () => this.call<WindowSize>("window.getSize"),
    minimize: () => this.call<void>("window.minimize"),
    maximize: () => this.call<void>("window.maximize"),
    restore: () => this.call<void>("window.restore"),
  };

  // System functions
  system = {
    getPlatform: () =>
      this.call<"macos" | "windows" | "linux">("system.getPlatform"),
    getConfig: () => this.call<AppConfig>("system.getConfig"),
  };

  // Streaming functions
  streaming = {
    getConfig: () =>
      this.call<{ enabled: boolean; port: number }>("streaming.getConfig"),
    getServerUrl: () => this.call<string>("streaming.getServerUrl"),
  };

  // Counter functions
  counter = {
    increment: () => this.call<number>("counter.increment"),
    decrement: () => this.call<number>("counter.decrement"),
    getValue: () => this.call<number>("counter.getValue"),
    setValue: (args: { value: number }) =>
      this.call<number>("counter.setValue", args),
  };

  // Demo functions
  demo = {
    greet: (args: { name?: string }) => this.call<string>("demo.greet", args),
  };

  // UI functions
  ui = {
    showAlert: (params?: {
      title?: string;
      message?: string;
      okButton?: string;
      cancelButton?: string;
    }) => this.call<boolean>("ui.showAlert", params),
  };
}

// Create and export bridge instance
const bridge = new Bridge();

// Make bridge available globally for debugging
if (typeof window !== "undefined") {
  window.bridge = bridge;
}

export { bridge };
export default bridge;
