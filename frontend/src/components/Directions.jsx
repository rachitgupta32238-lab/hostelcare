import { useState } from "react";
import { LocateFixed, Navigation } from "lucide-react";
import { getDirections } from "../services/api";

function formatDistance(meters) {
  return meters >= 1000 ? `${(meters / 1000).toFixed(1)} km` : `${meters} m`;
}
function formatDuration(seconds) {
  const minutes = Math.max(1, Math.round(seconds / 60));
  return minutes >= 60 ? `${Math.floor(minutes / 60)} h ${minutes % 60} min` : `${minutes} min`;
}

// Exact walking distance from the worker's current location
// (OpenRouteService via our server), with one-tap Google Maps navigation.
export default function Directions({ task }) {
  const [result, setResult] = useState(null);
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);

  async function load() {
    setBusy(true);
    setError("");
    try {
      if (!navigator.geolocation) throw new Error("This browser cannot share your location.");
      const position = await new Promise((resolve, reject) =>
        navigator.geolocation.getCurrentPosition(
          (p) => resolve({ latitude: p.coords.latitude, longitude: p.coords.longitude }),
          () => reject(new Error("Allow location access in your browser to get the distance.")),
          { enableHighAccuracy: true, timeout: 10000 }
        )
      );
      setResult(await getDirections(task.id, position));
    } catch (err) {
      setError(err.message);
    } finally {
      setBusy(false);
    }
  }

  const exact = Boolean(result) && result.source !== "campus-map";
  // The server's fallback message ends with a note about the campus map; show only the reason.
  const reason = (result?.message || "Exact distance is not available right now.").split(". Showing")[0];

  return (
    <div className="directions">
      <div className="directions-actions">
        <button className="ghost-btn" disabled={busy} onClick={load}>
          <LocateFixed size={14} /> {busy ? "Locating…" : result ? "Update my location" : "Get distance from my location"}
        </button>
        {result?.navigationUrl && (
          <a className="primary-btn" href={result.navigationUrl} target="_blank" rel="noreferrer">
            <Navigation size={14} /> Open in Google Maps
          </a>
        )}
      </div>

      {error && <p className="form-error">{error}</p>}

      {result && exact && (
        <div className="directions-result">
          <div className="distance-big">
            <b>{formatDistance(result.distanceMeters)}</b>
            {result.durationSeconds > 0 && (
              <span>{formatDuration(result.durationSeconds)} {result.travelMode === "WALK" ? "walk" : "travel"}</span>
            )}
            <small>From your current location to {result.destination?.name}</small>
          </div>
          {result.attribution && <small className="attribution">{result.attribution}</small>}
        </div>
      )}
      {result && !exact && <p className="hint">{reason}.</p>}
    </div>
  );
}