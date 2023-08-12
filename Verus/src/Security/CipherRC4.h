// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Security
{
	class CipherRC4
	{
	public:
		static inline void Encrypt(
			RcString password,
			const Vector<BYTE>& vData,
			Vector<BYTE>& vCipher,
			size_t skip = 787)
		{
			vCipher.resize(vData.size());
			size_t i, j, a;
			BYTE key[256];
			BYTE box[256];
			for (i = 0; i < 256; ++i)
			{
				key[i] = password[i % password.length()];
				box[i] = static_cast<BYTE>(i);
			}
			for (j = i = 0; i < 256; ++i)
			{
				j = (j + box[i] + key[i]) & 0xFF;
				std::swap(box[i], box[j]);
			}
			const size_t count = vData.size() + skip;
			for (a = j = i = 0; i < count; ++i)
			{
				a = (a + 1) & 0xFF;
				j = (j + box[a]) & 0xFF;
				std::swap(box[a], box[j]);
				const BYTE k = box[(box[a] + box[j]) & 0xFF];
				if (i >= skip)
					vCipher[i - skip] = vData[i - skip] ^ k;
			}
		}

		static inline void Decrypt(
			RcString password,
			const Vector<BYTE>& vCipher,
			Vector<BYTE>& vData,
			size_t skip = 787)
		{
			Encrypt(password, vCipher, vData, skip);
		}

		static inline void Test()
		{
			// See: https://tools.ietf.org/html/rfc6229

			const BYTE offset0000_256[] = { 0xEA, 0xA6, 0xBD, 0x25, 0x88, 0x0B, 0xF9, 0x3D, 0x3F, 0x5D, 0x1E, 0x4C, 0xA2, 0x61, 0x1D, 0x91 };
			const BYTE offset1536_256[] = { 0x3E, 0x34, 0x13, 0x5C, 0x79, 0xDB, 0x01, 0x02, 0x00, 0x76, 0x76, 0x51, 0xCF, 0x26, 0x30, 0x73 };
			String password;
			password.resize(32);
			VERUS_FOR(i, 32)
				password[i] = i + 1;
			Vector<BYTE> v, vCip;
			v.resize(16);
			Encrypt(password, v, vCip, 0);
			VERUS_RT_ASSERT(!memcmp(offset0000_256, vCip.data(), vCip.size()));
			Encrypt(password, v, vCip, 1536);
			VERUS_RT_ASSERT(!memcmp(offset1536_256, vCip.data(), vCip.size()));

			password.resize(5);
			const BYTE offset0768_40[] = { 0xEB, 0x62, 0x63, 0x8D, 0x4F, 0x0B, 0xA1, 0xFE, 0x9F, 0xCA, 0x20, 0xE0, 0x5B, 0xF8, 0xFF, 0x2B };
			const BYTE offset3072_40[] = { 0xEC, 0x0E, 0x11, 0xC4, 0x79, 0xDC, 0x32, 0x9D, 0xC8, 0xDA, 0x79, 0x68, 0xFE, 0x96, 0x56, 0x81 };
			Encrypt(password, v, vCip, 768);
			VERUS_RT_ASSERT(!memcmp(offset0768_40, vCip.data(), vCip.size()));
			Encrypt(password, v, vCip, 3072);
			VERUS_RT_ASSERT(!memcmp(offset3072_40, vCip.data(), vCip.size()));
		}
	};
}
