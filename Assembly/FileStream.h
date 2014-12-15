

#ifndef FILESTREAM_H_INCLUDED
#define FILESTREAM_H_INCLUDED

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#endif

#include <ObjIdlbase.h>
#include <windows.h>

class SHFileStream : public IStream {
private:
   volatile LONG m_cRef;
   HANDLE m_hFile;
   LPCWSTR m_fileName;

private:
   STDMETHODIMP Open(DWORD dwOpenMode);

public:
   SHFileStream(LPCWSTR fileName);
   ~SHFileStream();

   // Additional functions
   STDMETHODIMP OpenRead();
   STDMETHODIMP OpenWrite();
   STDMETHODIMP Close();

   // IUnknown functions
   STDMETHODIMP_(DWORD) AddRef();
   STDMETHODIMP_(DWORD) Release();
   STDMETHODIMP QueryInterface(const IID &riid, void **ppvObject);

   // ISequentialStream functions
   STDMETHODIMP Read(
      /* [annotation] */
      _Out_writes_bytes_to_(cb, *pcbRead)  void *pv,
      /* [annotation][in] */
      _In_  ULONG cb,
      /* [annotation] */
      _Out_opt_  ULONG *pcbRead);

   STDMETHODIMP Write(
      /* [annotation] */
      _In_reads_bytes_(cb)  const void *pv,
      /* [annotation][in] */
      _In_  ULONG cb,
      /* [annotation] */
      _Out_opt_  ULONG *pcbWritten);

   // IStream functions
   STDMETHODIMP Seek(
      /* [in] */ LARGE_INTEGER dlibMove,
      /* [in] */ DWORD dwOrigin,
      /* [annotation] */
      _Out_opt_  ULARGE_INTEGER *plibNewPosition);

   STDMETHODIMP SetSize(
      /* [in] */ ULARGE_INTEGER libNewSize);

   STDMETHODIMP CopyTo(
      /* [annotation][unique][in] */
      _In_  IStream *pstm,
      /* [in] */ ULARGE_INTEGER cb,
      /* [annotation] */
      _Out_opt_  ULARGE_INTEGER *pcbRead,
      /* [annotation] */
      _Out_opt_  ULARGE_INTEGER *pcbWritten);

   STDMETHODIMP Commit(
      /* [in] */ DWORD grfCommitFlags);

   STDMETHODIMP Revert(void);

   STDMETHODIMP LockRegion(
      /* [in] */ ULARGE_INTEGER libOffset,
      /* [in] */ ULARGE_INTEGER cb,
      /* [in] */ DWORD dwLockType);

   STDMETHODIMP UnlockRegion(
      /* [in] */ ULARGE_INTEGER libOffset,
      /* [in] */ ULARGE_INTEGER cb,
      /* [in] */ DWORD dwLockType);

   STDMETHODIMP Stat(
      /* [out] */ __RPC__out STATSTG *pstatstg,
      /* [in] */ DWORD grfStatFlag);

   STDMETHODIMP Clone(
      /* [out] */ __RPC__deref_out_opt IStream **ppstm);

};

#endif //FILESTREAM_H_INCLUDED
