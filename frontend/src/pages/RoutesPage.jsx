import { useEffect, useState } from "react";
import { Route } from "lucide-react";
import { getRouteLocations, getShortestRoute } from "../services/api";
import { RouteStops } from "../components/ui";

export default function RoutesPage({ onMessage }) {
  const [locations, setLocations] = useState([]);
  const [from, setFrom] = useState("");
  const [to, setTo] = useState("");
  const [route, setRoute] = useState(null);

  useEffect(() => {
    getRouteLocations()
      .then((d) => {
        const list = d.locations || [];
        setLocations(list);
        setFrom(String(list.find((l) => l.name === "Main Gate")?.id ?? 0));
        setTo(String(list.find((l) => l.name === "Atlantis Hostel")?.id ?? 1));
      })
      .catch((err) => onMessage(err.message, "error"));
  }, [onMessage]);

  async function find(e) {
    e.preventDefault();
    try {
      setRoute(await getShortestRoute(from, to));
    } catch (err) {
      onMessage(err.message, "error");
    }
  }

  return (
    <section className="panel route-panel">
      <div className="panel-heading">
        <div>
          <h3>Shortest campus route</h3>
          <p>Dijkstra's algorithm on the campus map from the original C++ CampusGraph.</p>
        </div>
        <Route />
      </div>
      <form className="form route-form" onSubmit={find}>
        <label><span>From</span>
          <select value={from} onChange={(e) => setFrom(e.target.value)}>
            {locations.map((l) => <option key={l.id} value={l.id}>{l.name}</option>)}
          </select>
        </label>
        <label><span>To</span>
          <select value={to} onChange={(e) => setTo(e.target.value)}>
            {locations.map((l) => <option key={l.id} value={l.id}>{l.name}</option>)}
          </select>
        </label>
        <button className="primary-btn" type="submit">Find route</button>
      </form>
      {route && <div className="route-result"><RouteStops route={route} /></div>}
    </section>
  );
}
