# Third-party notices for the Windows x64 package

This file supplements the individual license texts in the `licenses`
directory. The exact OpenJKDF2 source commit and its recursive submodule
revisions are recorded beside this file in `SOURCE_URL.txt`,
`SOURCE_COMMIT.txt`, and `SUBMODULES.txt`. That checkout contains the engine,
vendored, and submodule source. Ubuntu-supplied compiler runtime components are
identified separately by package and version in the `licenses` directory.

No LucasArts game data is included in the package.

## curl 8.0.0-DEV

The Windows target embeds the vendored curl sources under
`src/external/curl`. The following notice is reproduced from curl's COPYING
file for the corresponding 8.0-era source:

> COPYRIGHT AND PERMISSION NOTICE
>
> Copyright (c) 1996 - 2023, Daniel Stenberg, <daniel@haxx.se>, and many
> contributors, see the THANKS file.
>
> All rights reserved.
>
> Permission to use, copy, modify, and distribute this software for any purpose
> with or without fee is hereby granted, provided that the above copyright
> notice and this permission notice appear in all copies.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
> NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
> DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
> OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
> OR OTHER DEALINGS IN THE SOFTWARE.
>
> Except as contained in this notice, the name of a copyright holder shall not
> be used in advertising or otherwise to promote the sale, use or other dealings
> in this Software without prior written authorization of the copyright holder.

### ISC-licensed curl compatibility sources

The curl source set also contains ISC-licensed address-conversion and path
compatibility code.

Copyright (C) 1996-2022 Internet Software Consortium.

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

The second address-conversion source carries this form of the notice:

Copyright (c) Internet Software Consortium.

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE CONSORTIUM
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

The SSH-path code carries a separate ISC notice:

Copyright (c) 2001-2004 Damien Miller <djm@openbsd.org>.

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

### BSD-3-Clause curl Kerberos source

Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
(Royal Institute of Technology, Stockholm, Sweden).
Copyright (C) Daniel Stenberg.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the Institute nor the names of its contributors may be
   used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## Mbed TLS 3.3.0

The Windows target embeds the vendored Mbed TLS 3.3.0 sources under
`src/external/mbedtls`.

Copyright The Mbed TLS Contributors.

The sources identify themselves with SPDX license identifier `Apache-2.0`.
The complete Apache License, Version 2.0 is included as
`licenses/Apache-2.0.txt`.

## GNU Library/Lesser GPL components and corresponding source

Libsmacker is statically embedded in the engine and is licensed under the GNU
Lesser General Public License, Version 2.1. OpenAL Soft is supplied as a
separate DLL and is licensed under the GNU Library General Public License,
Version 2, with some portions under the included BSD 3-Clause terms. The
package includes the applicable license texts and the additional
compiled-source notices below.

`SOURCE_URL.txt` gives commands for obtaining the exact corresponding engine
source and all recursive submodules from the same GitHub fork. That checkout
can rebuild the engine and relink modified versions of these components.

### Additional OpenAL Soft notices

The compiled OpenAL Soft library includes the bs2b crossfeed implementation:

Copyright (c) 2005 Boris Mikhaylov.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

The compiled compressor and vocal-morpher effects also carry these
attributions under the included BSD 3-Clause terms:

- Copyright (C) 2013 by Anis A. Hireche.
- Copyright (C) 2019 by Anis A. Hireche.

The default HRTF table is generated from the full KEMAR HRIR set provided by
Bill Gardner <billg@media.mit.edu> and Keith Martin <kdm@media.mit.edu> of the
MIT Media Laboratory. The data is copyright 1994 MIT Media Laboratory and is
provided free of charge with no restrictions on use so long as those authors
are cited.

## SDL platform-header notices

SDL's compiled Windows support includes Microsoft D3D12 definitions:

Copyright (c) Microsoft Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

SDL also includes Vulkan API headers copyright 2015-2024 The Khronos Group
Inc., licensed under Apache-2.0. The full Apache License, Version 2.0 is in
`licenses/Apache-2.0.txt`.

## Mozilla-derived CA certificate bundle

The engine embeds `resource/ssl/cacert.pem`, a curl-format bundle extracted
from Mozilla NSS `certdata.txt` as of October 2, 2021. The exact source-form
bundle, including its generation metadata, is included as
`licenses/Mozilla-NSS-cacert-source.pem`. Mozilla's source is identified in the
bundle header and is available at:

<https://hg.mozilla.org/releases/mozilla-release/file/default/security/nss/lib/ckfw/builtins/certdata.txt>

Mozilla NSS source files are made available under the Mozilla Public License,
Version 2.0. A copy of that license can be obtained at:

<https://www.mozilla.org/MPL/2.0/>

## Statically linked MinGW runtime components

The executable statically links the MinGW-w64/GCC runtime archives listed in
`licenses/MinGW-static-runtime-PACKAGES.txt`. The same directory includes the
Debian/Ubuntu copyright and license records from every owning binary package.
Those compiler runtime sources are external to the OpenJKDF2 repository.

## Marco Paland tiny printf

The engine statically embeds the wide-character adaptation in `src/wprintf.c`
and `src/wprintf.h`.

Copyright (c) Marco Paland (info@paland.com), 2014-2019, PALANDesign Hannover,
Germany.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## CRC32 attribution

The engine's CRC32 implementation carries this notice:

Copyright (C) 1986 Gary S. Brown. You may use this program, or code or tables
extracted from it, as desired without restriction.

## MD5 provenance note

`src/General/md5.c` identifies itself as derived from the RSA Data Security,
Inc. MD5 Message-Digest Algorithm and modified into condensed control
structures. The audited upstream tree does not include a separate license file
for that implementation. This notice preserves the attribution present in the
source and makes no broader provenance claim.
