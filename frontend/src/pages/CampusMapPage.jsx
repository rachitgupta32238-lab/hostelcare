import { useCallback, useEffect, useState } from "react";
import { MapPin } from "lucide-react";
import { clearLocationCoordinates, getLocations, MAPS_EMBED_KEY, setLocationCoordinates } from "../services/api";

// Lets the warden attach real GPS coordinates to each campus place so the
// worker portal can show exact Google Maps distances and navigation.
export default function CampusMapPage({ onMessage }) {
  const [locations, setLocations] = useState([]);
  const [drafts, setDrafts] = useState({});
  const [preview, setPreview] = useState(null);

  const load = useCallback(async () => {
    try {
      const list = (await getLocations()).locations || [];
      setLocations(list);
      setDrafts(Object.fromEntries(list.map((l) => [l.id, l.hasCoordinates ? `${l.latitude}, ${l.longitude}` : ""])));
    } catch (err) {
      onMessage(err.message, "error");
    }
  }, [onMessage]);

  useEffect(() => { load(); }, [load]);

  async function save(location) {
    const text = (drafts[location.id] || "").trim();
    try {
      if (!text) {
        await clearLocationCoordinates(location.id);
      } else {
        const parts = text.split(",").map((p) => Number(p.trim()));
        if (parts.length !== 2 || parts.some((n) => Number.isNaN(n)))
          throw new Error("Paste coordinates as: latitude, longitude (e.g. 30.2681, 77.9952)");
        await setLocationCoordinates(location.id, parts[0], parts[1]);
      }
      onMessage(`${location.name} saved`, "success");
      load();
    } catch (err) {
      onMessage(err.message, "error");
    }
  }

  const missing = locations.filter((l) => !l.hasCoordinates).length;

  return (
    <>
      <section className="panel explain">
        <h3>How to add coordinates</h3>
        <p>
          Open Google Maps, right-click the exact building entrance, and click the numbers at the top of the menu to copy
          them. Paste them here as <b>latitude, longitude</b>. Workers then get exact road distance, travel time and
          one-tap navigation. Until a place has coordinates, the approximate campus-map distance is used.
        </p>
        {missing > 0 && <div className="alert">{missing} of {locations.length} places have no coordinates yet.</div>}
      </section>

      <div className="settings-grid">
        <section className="panel">
          <div className="table-wrap">
            <table>
              <thead><tr><th>Place</th><th>Latitude, longitude</th><th aria-label="Actions" /></tr></thead>
              <tbody>
                {locations.map((l) => (
                  <tr key={l.id}>
                    <td><b>{l.name}</b><small>{l.hasCoordinates ? "Set" : "Not set"}</small></td>
                    <td>
                      <input
                        className="coord-input"
                        aria-label={`Coordinates of ${l.name}`}
                        value={drafts[l.id] || ""}
                        placeholder="30.2681, 77.9952"
                        onChange={(e) => setDrafts({ ...drafts, [l.id]: e.target.value })}
                      />
                    </td>
                    <td className="actions-cell"><div className="actions">
                      <button className="ghost-btn" onClick={() => save(l)}>Save</button>
                      {l.hasCoordinates && <button className="ghost-btn" onClick={() => setPreview(l)}><MapPin size={14} /> View</button>}
                    </div></td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </section>

        <section className="panel">
          <h3 className="chart-title">{preview ? preview.name : "Map preview"}</h3>
          {!preview ? (
            <p className="muted">Click “View” next to a place to check its pin.</p>
          ) : MAPS_EMBED_KEY ? (
            <iframe
              className="map-frame"
              title={`Map of ${preview.name}`}
              src={`https://www.google.com/maps/embed/v1/place?key=${encodeURIComponent(MAPS_EMBED_KEY)}&q=${preview.latitude},${preview.longitude}&zoom=18`}
              loading="lazy"
              allowFullScreen
            />
          ) : (
            <a className="primary-btn" target="_blank" rel="noreferrer"
               href={`https://www.google.com/maps/search/?api=1&query=${preview.latitude},${preview.longitude}`}>
              Open {preview.name} in Google Maps
            </a>
          )}
        </section>
      </div>
    </>
  );
}
