import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/api': {
        target: 'http://my-brujula.local',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api/, '')
      },
      '/download': {
        target: 'http://my-brujula.local',
        changeOrigin: true,
      }
    }
  }
})
