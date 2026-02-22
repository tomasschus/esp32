FROM node:20-alpine

WORKDIR /app

COPY backend/package*.json ./
RUN npm install

COPY backend/. .
RUN npm run build && npm prune --production

ENV NODE_ENV=production
EXPOSE 4500
CMD ["node", "dist/server.js"]
