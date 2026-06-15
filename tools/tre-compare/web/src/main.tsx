import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { TooltipProvider } from '@/components/ui/tooltip'
import './index.css'
import App from './App.tsx'

// TanStack Query (chosen per executor discretion, RESEARCH Pitfall 4): the keyed /file/detail
// query dedups + discards stale on rapid file selection, and /compare/{set,files} caching makes
// a re-compare / re-select instant. retry:false — a backend 400/404 is a real, surfaceable
// error envelope, not a transient to retry.
const queryClient = new QueryClient({
  defaultOptions: {
    queries: { retry: false, refetchOnWindowFocus: false },
  },
})

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <QueryClientProvider client={queryClient}>
      <TooltipProvider>
        <App />
      </TooltipProvider>
    </QueryClientProvider>
  </StrictMode>,
)
