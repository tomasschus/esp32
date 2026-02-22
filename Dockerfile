FROM --platform=linux/amd64 node:20-alpine

WORKDIR /app

COPY backend/package*.json ./
RUN npm install

COPY backend/. .
RUN npm run build

EXPOSE 4500
CMD ["node", "dist/server.js"]
