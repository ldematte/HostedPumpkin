
#include "FileStream.h"

SHFileStream::SHFileStream(LPCWSTR fileName) {
   m_cRef = 0;
   m_fileName = fileName;
   m_hFile = INVALID_HANDLE_VALUE;
}

SHFileStream::~SHFileStream() {}

// IUnknown functions

STDMETHODIMP_(DWORD) SHFileStream::AddRef() {
   return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(DWORD) SHFileStream::Release() {
   ULONG cRef = InterlockedDecrement(&m_cRef);
   if (cRef == 0) {
      delete this;
   }
   return cRef;
}

STDMETHODIMP SHFileStream::QueryInterface(const IID &riid, void **ppvObject) {
   if (!ppvObject)
      return E_POINTER;

   if (riid == IID_IUnknown || riid == IID_IStream) {
      *ppvObject = this;
      AddRef();
      return S_OK;
   }

   *ppvObject = NULL;
   return E_NOINTERFACE;
}

STDMETHODIMP SHFileStream::Open(DWORD dwAccessMode) {
   m_hFile = ::CreateFile(m_fileName, dwAccessMode, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
   if (m_hFile == INVALID_HANDLE_VALUE)
      return HRESULT_FROM_WIN32(GetLastError());

   return S_OK;
}

STDMETHODIMP SHFileStream::OpenRead() {
   return Open(GENERIC_READ);
}

STDMETHODIMP SHFileStream::OpenWrite() {
   return Open(GENERIC_READ | GENERIC_WRITE);
}

STDMETHODIMP SHFileStream::Close() {
   if (m_hFile != INVALID_HANDLE_VALUE)
      ::CloseHandle(m_hFile);
   m_hFile = NULL;

   return S_OK;
}

// ISequentialStream functions
STDMETHODIMP SHFileStream::Read(
   /* [annotation] */
   _Out_writes_bytes_to_(cb, *pcbRead)  void *pv,
   /* [annotation][in] */
   _In_  ULONG cb,
   /* [annotation] */
   _Out_opt_  ULONG *pcbRead) {

   if (::ReadFile(m_hFile, pv, cb, pcbRead, NULL))
      return S_OK;
   else
      return HRESULT_FROM_WIN32(GetLastError());
}

STDMETHODIMP SHFileStream::Write(
   /* [annotation] */
   _In_reads_bytes_(cb)  const void *pv,
   /* [annotation][in] */
   _In_  ULONG cb,
   /* [annotation] */
   _Out_opt_  ULONG *pcbWritten) { 

   if (::WriteFile(m_hFile, pv, cb, pcbWritten, NULL))
      return S_OK;
   else
      return HRESULT_FROM_WIN32(GetLastError());
}


// IStream functions
STDMETHODIMP SHFileStream::Seek(
   /* [in] */ LARGE_INTEGER dlibMove,
   /* [in] */ DWORD dwOrigin,
   /* [annotation] */
   _Out_opt_  ULARGE_INTEGER *plibNewPosition) {

   // The origin can be the beginning of the file (STREAM_SEEK_SET), 
   // the current seek pointer (STREAM_SEEK_CUR), or the end of the file (STREAM_SEEK_END)
   DWORD dwMoveMethod;
   if (dwOrigin == STREAM_SEEK_SET) {
      dwMoveMethod = FILE_BEGIN;
   }
   else if (dwOrigin == STREAM_SEEK_CUR) {
      dwMoveMethod = FILE_CURRENT;
   }
   else if (dwOrigin == STREAM_SEEK_END) {
      dwMoveMethod = FILE_END;
   }
   else {
      return E_INVALIDARG;
   }

   LONG distanceToMoveHigh = 0;
   PLONG lpDistanceToMoveHigh = NULL;
   if (dlibMove.HighPart > 0) {
      lpDistanceToMoveHigh = &distanceToMoveHigh;
      distanceToMoveHigh = dlibMove.HighPart;
   }
   DWORD result = SetFilePointer(m_hFile, dlibMove.LowPart, lpDistanceToMoveHigh, dwMoveMethod);

   if (result == INVALID_SET_FILE_POINTER && (GetLastError() != NO_ERROR))
      return HRESULT_FROM_WIN32(GetLastError());
   

   if (plibNewPosition != NULL) {
      plibNewPosition->LowPart = result;
      plibNewPosition->HighPart = distanceToMoveHigh;
   }

   return S_OK;
}

STDMETHODIMP SHFileStream::SetSize(
   /* [in] */ ULARGE_INTEGER libNewSize) {
   HRESULT hr = S_OK;

   LONG currentPositionHigh = 0;
   DWORD currentPositionLow = SetFilePointer(m_hFile, 0, &currentPositionHigh, FILE_CURRENT);

   if (currentPositionLow == INVALID_SET_FILE_POINTER && (GetLastError() != NO_ERROR))
      return HRESULT_FROM_WIN32(GetLastError());

   LONG positionHigh = libNewSize.HighPart;
   DWORD positionLow = SetFilePointer(m_hFile, libNewSize.LowPart, &positionHigh, FILE_CURRENT);

   if (positionLow == INVALID_SET_FILE_POINTER && (GetLastError() != NO_ERROR))
      return HRESULT_FROM_WIN32(GetLastError());
   
   BOOL result = SetEndOfFile(m_hFile);
   if (!result) {
      hr = HRESULT_FROM_WIN32(GetLastError());
   }

   // Put the pointer back
   SetFilePointer(m_hFile, currentPositionLow, &currentPositionHigh, FILE_CURRENT);

   return hr;
}

STDMETHODIMP SHFileStream::CopyTo(
   /* [annotation][unique][in] */
   _In_  IStream *pstm,
   /* [in] */ ULARGE_INTEGER cb,
   /* [annotation] */
   _Out_opt_  ULARGE_INTEGER *pcbRead,
   /* [annotation] */
   _Out_opt_  ULARGE_INTEGER *pcbWritten) {

   char buffer[4096];
   ULONGLONG bytesToWrite = cb.QuadPart;   
   while (bytesToWrite > 0) {
      DWORD cbRead = 0, cbWritten = 0;
      if (!::ReadFile(m_hFile, buffer, sizeof(buffer), &cbRead, NULL))
         return HRESULT_FROM_WIN32(GetLastError());

      if (pcbRead)
         pcbRead->QuadPart += cbRead;      

      HRESULT hr = pstm->Write(buffer, cbRead, &cbWritten);
      if (FAILED(hr))
         return hr;

      if (pcbWritten)
         pcbWritten->QuadPart += cbWritten;

      if (cbRead < sizeof(buffer))
         //not enough to read
         break;

      bytesToWrite -= cbRead;
   }

   return S_OK;
}

STDMETHODIMP SHFileStream::Commit(
   /* [in] */ DWORD /*grfCommitFlags*/) {
   FlushFileBuffers(m_hFile);
   return S_OK;
}

STDMETHODIMP SHFileStream::Revert(void) {
   // Our stream id direct.
   // According to the docs, this value is ignored
   // http://msdn.microsoft.com/en-us/library/windows/desktop/aa380042%28v=vs.85%29.aspx

   return S_OK;
}

STDMETHODIMP SHFileStream::LockRegion(
   /* [in] */ ULARGE_INTEGER libOffset,
   /* [in] */ ULARGE_INTEGER cb,
   /* [in] */ DWORD /*dwLockType*/) {

   if (::LockFile(m_hFile, libOffset.LowPart, libOffset.HighPart, cb.LowPart, cb.HighPart))
      return S_OK;
   else
      return HRESULT_FROM_WIN32(GetLastError());
}

STDMETHODIMP SHFileStream::UnlockRegion(
   /* [in] */ ULARGE_INTEGER libOffset,
   /* [in] */ ULARGE_INTEGER cb,
   /* [in] */ DWORD /*dwLockType*/) {

   if (::UnlockFile(m_hFile, libOffset.LowPart, libOffset.HighPart, cb.LowPart, cb.HighPart))
      return S_OK;
   else
      return HRESULT_FROM_WIN32(GetLastError());
}

STDMETHODIMP SHFileStream::Stat(
   /* [out] */ __RPC__out STATSTG *pstatstg,
   /* [in] */ DWORD grfStatFlag) {

   if (!(grfStatFlag & STATFLAG_NONAME)) {
      int len = lstrlenW(m_fileName);
      pstatstg->pwcsName = (LPOLESTR)CoTaskMemAlloc(len);
      lstrcpynW(pstatstg->pwcsName, m_fileName, len);
   }

   pstatstg->type = STGTY_STREAM;
   pstatstg->cbSize.LowPart = GetFileSize(m_hFile, &(pstatstg->cbSize.HighPart));
   
   GetFileTime(m_hFile, &(pstatstg->ctime), &(pstatstg->atime), &(pstatstg->mtime));

   pstatstg->grfMode = 0;
   pstatstg->grfLocksSupported = LOCK_EXCLUSIVE;

   // Not used for IStream
   //pstatstg->grfStateBits;
   //pstatstg->clsid;

   return S_OK;
}

STDMETHODIMP SHFileStream::Clone(
   /* [out] */ __RPC__deref_out_opt IStream **ppstm) {

   if (!ppstm)
      return STG_E_INVALIDPOINTER;

   SHFileStream* other = new SHFileStream(m_fileName);

   // Get the current position
   LONG currentPositionHigh = 0;
   DWORD currentPositionLow = SetFilePointer(m_hFile, 0, &currentPositionHigh, FILE_CURRENT);
   // ... if we can
   if (!(currentPositionLow == INVALID_SET_FILE_POINTER && (GetLastError() != NO_ERROR))) {
      LARGE_INTEGER move;
      move.LowPart = currentPositionLow;
      move.HighPart = currentPositionHigh;
      other->Seek(move, STREAM_SEEK_SET, NULL);
   }

   *ppstm = other;
   return S_OK;
}
