FROM oven/bun:latest

WORKDIR /app

COPY backend/package.json backend/bun.lock ./
RUN bun install --frozen-lockfile

COPY backend/. .
RUN bun run build

ENV NODE_ENV=production
EXPOSE 4500
ENTRYPOINT ["bun", "dist/main.js"]
