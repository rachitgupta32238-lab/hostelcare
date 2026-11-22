# ---------- Build the React app ----------
FROM node:22-bookworm-slim AS build
WORKDIR /app
COPY package.json ./
# package-lock.json is not copied: it was generated on Windows and may miss
# Linux-only optional packages used by Vite.
RUN npm install --no-audit --no-fund
COPY index.html vite.config.js ./
COPY src ./src
ARG VITE_GOOGLE_MAPS_EMBED_KEY=""
ENV VITE_API_BASE=/api \
    VITE_GOOGLE_MAPS_EMBED_KEY=$VITE_GOOGLE_MAPS_EMBED_KEY
RUN npm run build

# ---------- Serve with Caddy (automatic HTTPS) ----------
FROM caddy:2
COPY --from=build /app/dist /srv
COPY Caddyfile /etc/caddy/Caddyfile
