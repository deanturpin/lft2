import { mount } from 'svelte'
import App from './App.svelte'
import './app.css'

// Attempt to lock screen orientation to landscape (only works in fullscreen)
if (screen.orientation && screen.orientation.lock) {
  screen.orientation.lock('landscape').catch(() => {
    // Silently fail if not supported or not in fullscreen
  })
}

const app = mount(App, {
  target: document.getElementById('app')
})

export default app
