#ifndef MD5_H
#define MD5_H

/* MD5 declaration. */
class MD5 {
public:
	MD5();
	MD5(const void* input, size_t length);
	void update(const void* input, size_t length);
	const BYTE* digest();
	CString toString();
	void reset();

private:
	void update(const BYTE* input, size_t length);
	void final();
	void transform(const BYTE block[64]);
	void encode(const DWORD* input, BYTE* output, size_t length);
	void decode(const BYTE* input, DWORD* output, size_t length);
	CString bytesToHexString(const BYTE* input, size_t length);

	/* class uncopyable */
	MD5(const MD5&);
	MD5& operator=(const MD5&);

private:
	DWORD m_dwState[4];	/* state (ABCD) */
	DWORD m_dwCount[2];	/* number of bits, modulo 2^64 (low-order word first) */
	BYTE m_Buffer[64];	/* input buffer */
	BYTE m_Digest[16];	/* message digest */
	BOOL m_bFinished;		/* calculate finished ? */

	static const BYTE PADDING[64];	/* padding for calculate */
	static const char HEX[16];
	enum { BUFFER_SIZE = 1024 };
};

#endif /*MD5_H*/
