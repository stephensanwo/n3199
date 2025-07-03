// Modern Bridge implementation for frontend
// Clean sidebar-only API without legacy drawer support
import type {
  BridgeAPI,
  BridgeMessage,
  BridgeCallback,
  WindowSize,
  AppConfig,
  SidebarState,
} from "./bridge.d";

// Re-export all types for convenience
export type {
  BridgeAPI,
  BridgeMessage,
  BridgeCallback,
  WindowSize,
  AppConfig,
  SidebarState,
} from "./bridge.d";

class Bridge implements BridgeAPI {
  private nextCallbackId = 1;
  private callbacks = new Map<number, BridgeCallback>();

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

    // Set up native sidebar state change handler
    window.nativeSidebar = {
      onStateChange: (visible: boolean) => {
        // Dispatch custom event for sidebar state changes
        const event = new CustomEvent("sidebarStateChanged", {
          detail: { visible },
        });
        window.dispatchEvent(event);

        console.log(
          `Native sidebar state changed: ${visible ? "visible" : "hidden"}`
        );
      },
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

  // Window functions
  window = {
    setSize: (size: WindowSize) => this.call<void>("window.setSize", size),
    getSize: () => this.call<WindowSize>("window.getSize"),
    minimize: () => this.call<void>("window.minimize"),
    maximize: () => this.call<void>("window.maximize"),
    restore: () => this.call<void>("window.restore"),
  };

  // Modern Sidebar functions (NSSplitViewController)
  sidebar = {
    toggle: () => this.call<void>("sidebar.toggle"),
    show: () => this.call<void>("sidebar.show"),
    hide: () => this.call<void>("sidebar.hide"),
    getState: () => this.call<SidebarState>("sidebar.getState"),
  };

  // System functions
  system = {
    getPlatform: () =>
      this.call<"macos" | "windows" | "linux">("system.getPlatform"),
    getConfig: () => this.call<AppConfig>("system.getConfig"),
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
}

// Augment window interface
declare global {
  interface Window {
    handleBridgeResponse: (
      id: number,
      success: boolean,
      result: unknown
    ) => void;
    nativeSidebar?: {
      onStateChange(visible: boolean): void;
    };
  }
}

// Create and export bridge instance
const bridge = new Bridge();

// Make bridge available globally for debugging
if (typeof window !== "undefined") {
  window.bridge = bridge;
}

export { bridge };
export default bridge;
