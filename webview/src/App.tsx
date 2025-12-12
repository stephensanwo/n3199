import { useState, useEffect } from "react";
import reactLogo from "./assets/react.svg";
import viteLogo from "/vite.svg";
import "./App.css";
import bridge from "./bridge/bridge";
import StreamingData from "./components/StreamingData";
import TcpDumpStream from "./components/TcpDumpStream";

function App() {
  const [count, setCount] = useState(0);
  const [greeting, setGreeting] = useState("");
  const [error, setError] = useState("");
  // NEW: Search state for native toolbar events
  const [isSearching, setIsSearching] = useState(false);
  const [searchMessage, setSearchMessage] = useState("");
  // NEW: State for native alert result
  const [message, setMessage] = useState("");

  // Initialize counter from C backend
  useEffect(() => {
    console.log("Initializing counter from C backend...");
    bridge.counter
      .getValue()
      .then((value: number) => {
        console.log("Initial counter value:", value);
        setCount(value);
      })
      .catch((err: Error) => {
        console.error("Failed to get initial counter value:", err);
        setError(`Failed to initialize: ${err.message}`);
      });
  }, []);

  // NEW: Set up native event listeners
  useEffect(() => {
    console.log("Setting up native event listeners...");

    // Handle search events from native toolbar
    const handleOpenSearch = () => {
      console.log("Native search event received!");
      setIsSearching(true);
      setSearchMessage("Searching...");

      // Simulate search operation
      setTimeout(() => {
        setSearchMessage("Search results would appear here");
      }, 1500);
    };

    // Handle settings events
    const handleOpenSettings = () => {
      console.log("Native settings event received!");
      alert("Settings panel would open here");
    };

    // Handle share events
    const handleShowShare = () => {
      console.log("Native share event received!");
      alert("Share options would appear here");
    };

    // Handle favorites events
    const handleToggleFavorites = () => {
      console.log("Native favorites event received!");
      alert("Favorites toggled!");
    };

    // Register event listeners
    bridge.addEventListener("open_search", handleOpenSearch);
    bridge.addEventListener("open_settings", handleOpenSettings);
    bridge.addEventListener("show_share", handleShowShare);
    bridge.addEventListener("toggle_favorites", handleToggleFavorites);

    // Cleanup listeners on unmount
    return () => {
      bridge.removeEventListener("open_search", handleOpenSearch);
      bridge.removeEventListener("open_settings", handleOpenSettings);
      bridge.removeEventListener("show_share", handleShowShare);
      bridge.removeEventListener("toggle_favorites", handleToggleFavorites);
    };
  }, []);

  // Handle increment
  const handleIncrement = async () => {
    try {
      console.log("Incrementing counter...");
      const newCount = await bridge.counter.increment();
      console.log("New counter value:", newCount);
      setCount(newCount);
      setError(""); // Clear any previous errors
    } catch (err) {
      console.error("Failed to increment:", err);
      setError(
        `Increment failed: ${err instanceof Error ? err.message : String(err)}`
      );
    }
  };

  // Handle decrement
  const handleDecrement = async () => {
    try {
      console.log("Decrementing counter...");
      const newCount = await bridge.counter.decrement();
      console.log("New counter value:", newCount);
      setCount(newCount);
      setError(""); // Clear any previous errors
    } catch (err) {
      console.error("Failed to decrement:", err);
      setError(
        `Decrement failed: ${err instanceof Error ? err.message : String(err)}`
      );
    }
  };

  // Handle greeting
  const handleGreet = async () => {
    try {
      console.log("Requesting greeting from C backend...");
      const message = await bridge.demo.greet({ name: "React" });
      console.log("Received greeting:", message);
      setGreeting(message);
      setError(""); // Clear any previous errors
    } catch (err) {
      console.error("Failed to get greeting:", err);
      setError(
        `Greeting failed: ${err instanceof Error ? err.message : String(err)}`
      );
    }
  };

  // NEW: Handle native alert
  const handleShowNativeAlert = async () => {
    try {
      const result = await bridge.ui.showAlert({
        title: "Custom Alert",
        message:
          "This is a customizable native alert dialog with custom title, message, and buttons!",
        okButton: "Awesome!",
        cancelButton: "Not Now",
      });

      if (result) {
        console.log("User clicked 'Awesome!' button");
        setMessage("User clicked 'Awesome!' - Alert returned true");
      } else {
        console.log("User clicked 'Not Now' button");
        setMessage("User clicked 'Not Now' - Alert returned false");
      }
    } catch (error) {
      console.error("Failed to show native alert:", error);
      setMessage("Failed to show native alert");
    }
  };

  // NEW: Close search function
  const handleCloseSearch = () => {
    setIsSearching(false);
    setSearchMessage("");
  };

  return (
    <div>
      {/* NEW: Search Header - shows when native search toolbar button is clicked */}
      {isSearching && (
        <div
          style={{
            position: "fixed",
            top: 0,
            left: 0,
            right: 0,
            backgroundColor: "#007acc",
            color: "white",
            padding: "1rem",
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
            zIndex: 1000,
            boxShadow: "0 2px 4px rgba(0,0,0,0.2)",
          }}
        >
          <div>
            <strong>🔍 {searchMessage}</strong>
          </div>
          <button
            onClick={handleCloseSearch}
            style={{
              background: "transparent",
              border: "1px solid white",
              color: "white",
              padding: "0.25rem 0.5rem",
              cursor: "pointer",
              borderRadius: "4px",
            }}
          >
            ✕ Close
          </button>
        </div>
      )}

      {/* Main content with top margin when search is active */}
      <div
        style={{
          marginTop: isSearching ? "80px" : "0",
          transition: "margin-top 0.3s ease",
          padding: "1rem",
        }}
      >
        <div>
          <a href="https://vitejs.dev" target="_blank">
            <img src={viteLogo} className="logo" alt="Vite logo" />
          </a>
          <a href="https://react.dev" target="_blank">
            <img src={reactLogo} className="logo react" alt="React logo" />
          </a>
        </div>
        <h1>Vite + React + C Bridge</h1>

        {error && (
          <div
            style={{
              color: "red",
              margin: "1rem",
              padding: "0.5rem",
              border: "1px solid red",
              borderRadius: "4px",
            }}
          >
            Error: {error}
          </div>
        )}

        <div className="card">
          <button onClick={handleDecrement}>-</button>
          <span
            style={{ margin: "0 1rem", fontSize: "1.5rem", fontWeight: "bold" }}
          >
            Counter: {count}
          </span>
          <button onClick={handleIncrement}>+</button>
          <p>
            Edit <code>src/App.tsx</code> and save to test HMR
          </p>
        </div>

        <div className="card">
          <button onClick={handleGreet}>Greet from C Backend</button>
          {greeting && (
            <div
              style={{
                marginTop: "1rem",
                padding: "1rem",
                backgroundColor: "#262626",
                borderRadius: "4px",
              }}
            >
              <strong>Response from C:</strong> {greeting}
            </div>
          )}
        </div>

        {/* Native Alert Button */}
        <div className="card">
          <button onClick={handleShowNativeAlert}>Show Native Alert</button>
          {message && (
            <div
              style={{
                marginTop: "1rem",
                padding: "1rem",
                backgroundColor: "#262626",
                borderRadius: "4px",
                border: "1px solid #2196F3",
              }}
            >
              <strong>Native Alert Result:</strong> {message}
            </div>
          )}
        </div>

        {/* NEW: Streaming Data Component */}
        <StreamingData />

        {/* NEW: TCP Dump Stream Component */}
        <TcpDumpStream />

        {/* NEW: Instructions for testing native toolbar */}
        <div className="card">
          <h3>🔗 Native Toolbar Integration</h3>
          <p>
            Try clicking the toolbar buttons to see bidirectional communication:
          </p>
          <ul
            style={{ textAlign: "left", maxWidth: "400px", margin: "0 auto" }}
          >
            <li>
              <strong>🔍 Search:</strong> Shows search header above
            </li>
            <li>
              <strong>⚙️ Settings:</strong> Shows settings alert
            </li>
            <li>
              <strong>📤 Share:</strong> Shows share alert
            </li>
            <li>
              <strong>⭐ Star:</strong> Shows favorites alert
            </li>
            <li>
              <strong>↻ Refresh:</strong> Reloads the webview
            </li>
            <li>
              <strong>← →:</strong> Navigate back/forward
            </li>
          </ul>
        </div>

        {/* Debug section for layout verification */}
        <div
          className="card"
          style={{ backgroundColor: "#f9f9f9", border: "2px dashed #ccc" }}
        >
          <h4>🔧 Layout Debug Info</h4>
          <p style={{ fontSize: "0.9em", color: "#666" }}>
            This content should be properly separated from the native macOS
            toolbar above.
            <br />
            The webview now uses a container layout that respects toolbar
            boundaries.
          </p>
          <div
            style={{
              background: "linear-gradient(90deg, #ff6b6b, #4ecdc4, #45b7d1)",
              height: "20px",
              borderRadius: "10px",
              margin: "10px 0",
            }}
          />
          <small style={{ color: "#888" }}>
            If you see this content overlapping with toolbar buttons, the layout
            needs adjustment.
          </small>
        </div>

        <p className="read-the-docs">
          Click on the Vite and React logos to learn more
        </p>
      </div>
    </div>
  );
}

export default App;
