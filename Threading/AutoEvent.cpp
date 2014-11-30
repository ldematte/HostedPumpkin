#include "AutoEvent.h"

#include "../Logger.h"

SHAutoEvent::SHAutoEvent(SIZE_T cookie)
{
   m_cRef = 0;
   m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!m_hEvent)
      Logger::Critical("Error creating auto event: %d", GetLastError());
   m_cookie = cookie;
   m_pContext = NULL;
}

SHAutoEvent::~SHAutoEvent()
{
   if (m_hEvent)
   {
      CloseHandle(m_hEvent);
      m_hEvent = NULL;
   }
}

// IUnknown functions

STDMETHODIMP_(DWORD) SHAutoEvent::AddRef()
{
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHAutoEvent::Release()
{
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0)
      delete this;
   return cRef;
}

STDMETHODIMP SHAutoEvent::QueryInterface(const IID &riid, void **ppvObject)
{
   if (riid == IID_IUnknown || riid == IID_IHostAutoEvent)
   {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

// IHostManualEvent functions

STDMETHODIMP SHAutoEvent::Set() {

   Logger::Info("AutoEvent::Set");
   if (!SetEvent(m_hEvent))
   {
      DWORD error = GetLastError();
      Logger::Error("SetEvent error: %d", error);
      return HRESULT_FROM_WIN32(error);
   }

   if (m_pContext)
      m_pContext->Release(m_cookie);

   return S_OK;
}

STDMETHODIMP SHAutoEvent::Wait(DWORD dwMilliseconds, DWORD option)
{
   Logger::Info("AutoEvent::Wait");
   if (!m_pContext || option & WAIT_NOTINDEADLOCK)
   {
      return DHContext::HostWait(m_hEvent, dwMilliseconds, option);
   }
   else
   {
      DWORD dwCurrentTimeout = 100;
      DWORD dwTotalWait = 0;

      // Before doing deadlock detection, we attempt to wait for 100ms. If the event is
      // signaled, this avoids any expensive wait-graph traversal. Only if this times out
      // do we proceed with the more expensive checks.
   try_to_wait:
      HRESULT hr = DHContext::HostWait(m_hEvent, min(dwCurrentTimeout, dwMilliseconds), option);

      if (hr == HOST_E_TIMEOUT)
      {
         DHDetails *pDetails = m_pContext->TryEnter(m_cookie);

         if (pDetails)
         {
            // If the return value was non-null, we encountered a deadlock. Respond by
            // printing out some information and returning HOST_E_DEADLOCK.
            m_pContext->PrintDeadlock(pDetails, m_cookie);
            hr = HOST_E_DEADLOCK;
         }
         else
         {
            // Add the last wait time to our cumulative wait time.
            dwTotalWait += dwCurrentTimeout;
            if (dwTotalWait >= dwMilliseconds)
            {
               // We've already waited long enough. Exit with a HOST_E_TIMEOUT return-value.
               hr = HOST_E_TIMEOUT;
            }
            else
            {
               // We still haven't exceeded the user's millisecond timeout. Check to see if
               // we're past its half-way point. If yes, wait for the remainder (or if it's
               // infinite, just keep the same timeout). If we're not past its half-way
               // point, just double the timeout and try again.
               if (dwTotalWait >= dwMilliseconds / 2)
               {
                  if (dwMilliseconds != INFINITE)
                     dwCurrentTimeout = dwMilliseconds - dwTotalWait;
               }
               else
               {
                  dwCurrentTimeout *= 2;
               }
               goto try_to_wait;
            }
         }

         // Ensure to exit the context.
         m_pContext->EndEnter(m_cookie);
      }

      return hr;
   }
}