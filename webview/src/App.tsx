import { useState, useEffect } from "react";
import reactLogo from "./assets/react.svg";
import viteLogo from "/vite.svg";
import "./App.css";
import bridge from "./bridge/bridge";

function App() {
  const [count, setCount] = useState(0);
  const [greeting, setGreeting] = useState("");
  const [error, setError] = useState("");
  const [sidebarVisible, setSidebarVisible] = useState(false);

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

  // Get initial sidebar state
  useEffect(() => {
    console.log("Getting initial sidebar state...");
    bridge.sidebar
      .getState()
      .then((state) => {
        console.log("Initial sidebar state:", state);
        setSidebarVisible(state.visible);
      })
      .catch((err: Error) => {
        console.error("Failed to get sidebar state:", err);
      });
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

  // Sidebar Handlers
  const handleSidebarToggle = async () => {
    try {
      console.log("Toggling sidebar...");
      await bridge.sidebar.toggle();
      const state = await bridge.sidebar.getState();
      setSidebarVisible(state.visible);
      setError(""); // Clear any previous errors
    } catch (err) {
      console.error("Failed to toggle sidebar:", err);
      setError(
        `Sidebar toggle failed: ${
          err instanceof Error ? err.message : String(err)
        }`
      );
    }
  };

  const handleSidebarShow = async () => {
    try {
      console.log("Showing sidebar...");
      await bridge.sidebar.show();
      const state = await bridge.sidebar.getState();
      setSidebarVisible(state.visible);
      setError(""); // Clear any previous errors
    } catch (err) {
      console.error("Failed to show sidebar:", err);
      setError(
        `Sidebar show failed: ${
          err instanceof Error ? err.message : String(err)
        }`
      );
    }
  };

  const handleSidebarHide = async () => {
    try {
      console.log("Hiding sidebar...");
      await bridge.sidebar.hide();
      const state = await bridge.sidebar.getState();
      setSidebarVisible(state.visible);
      setError(""); // Clear any previous errors
    } catch (err) {
      console.error("Failed to hide sidebar:", err);
      setError(
        `Sidebar hide failed: ${
          err instanceof Error ? err.message : String(err)
        }`
      );
    }
  };

  return (
    <>
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
              backgroundColor: "#f0f0f0",
              borderRadius: "4px",
            }}
          >
            <strong>Response from C:</strong> {greeting}
          </div>
        )}
      </div>

      <div className="card">
        <h3>Sidebar Controls</h3>
        <div style={{ marginBottom: "1rem" }}>
          <strong>Sidebar Status:</strong>{" "}
          {sidebarVisible ? "Visible" : "Hidden"}
        </div>
        <div style={{ display: "flex", gap: "0.5rem", flexWrap: "wrap" }}>
          <button onClick={handleSidebarToggle}>Toggle Sidebar</button>
          <button onClick={handleSidebarShow}>Show Sidebar</button>
          <button onClick={handleSidebarHide}>Hide Sidebar</button>
        </div>
      </div>

      <p className="read-the-docs">
        Click on the Vite and React logos to learn more
      </p>
    </>
  );
}

export default App;
