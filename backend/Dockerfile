FROM node:20-alpine
WORKDIR /app

COPY backend/package*.json ./
RUN npm install --include=dev
COPY backend/tsconfig.json ./
COPY backend/src ./src
RUN node_modules/.bin/tsc && ls /app/dist
RUN npm prune --production && rm -rf src tsconfig.json

EXPOSE 4500
CMD ["node", "dist/server.js"]
