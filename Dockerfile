FROM node:20-alpine
WORKDIR /app
COPY backend/package*.json ./
RUN npm install --omit=dev
RUN npm install -g typescript
COPY backend/tsconfig.json ./
COPY backend/src ./src
RUN tsc && ls /app/dist
RUN rm -rf tsconfig.json src
EXPOSE 4500
CMD ["node", "dist/server.js"]
