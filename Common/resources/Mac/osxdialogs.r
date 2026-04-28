
/*	$Id$    */

/*
    SPDX-License-Identifier: MIT

    Copyright (c) 1992-2004 UserLand Software, Inc.
    Copyright (c) 2025-2026 Frontier contributors

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "frontier.r"


data 'DLOG' (518, purgeable) {
	$"00DE 0014 0159 01D7 0004 0000 0100 0000"            /* .Þ...Y.×........ */
	$"0000 0206 1546 696E 6420 2620 5265 706C"            /* .....Find & Repl */
	$"6163 6520 4469 616C 6F67 0000"                      /* ace Dialog.. */
};

data 'DLOG' (1024, purgeable) {
	$"0136 0191 01C1 02CD 0001 0000 0000 0000"            /* .6..Á.Í........ */
	$"0000 0400 00"                                       /* ..... */
};

data 'DLOG' (1025, purgeable) {
	$"0136 0191 01B9 028D 0000 0000 0000 0000"            /* .6..¹......... */
	$"0000 0401 00"                                       /* ..... */
};

data 'DLOG' (1026, purgeable) {
	$"0136 0191 0230 02EF 0000 0000 0000 0000"            /* .6..0.ï........ */
	$"0000 0402 00"                                       /* ..... */
};

data 'DLOG' (1027, purgeable) {
	$"00A8 00AC 00FB 019D 0003 0100 0100 0000"            /* .¨.¬.û......... */
	$"0000 0403 00"                                       /* ..... */
};

data 'DITL' (518, purgeable) {
	$"000B 0000 0000 005B 0154 006F 01B1 0404"            /* .......[.T.o.±.. */
	$"4669 6E64 0000 0000 009E 000B 00B2 0051"            /* Find........².Q */
	$"0406 4361 6E63 656C 0000 0000 000B 0154"            /* ..Cancel.......T */
	$"001F 01B1 040B 5265 706C 6163 6520 416C"            /* ...±..Replace Al */
	$"6C00 0000 0000 0028 0154 003C 01B1 0407"            /* l......(.T.<.±.. */
	$"5265 706C 6163 6574 0000 0000 000D 000A"            /* Replacet.......Â */
	$"001D 003A 8805 4669 6E64 3A07 0000 0000"            /* ...:.Find:..... */
	$"000D 004E 001D 013D 1004 4669 6E64 0000"            /* ...N...=..Find.. */
	$"0000 0029 000A 0039 0046 8808 5265 706C"            /* ...).Â.9.F.Repl */
	$"6163 653A 0000 0000 0029 004E 0039 013D"            /* ace:.....).N.9.= */
	$"1007 5265 706C 6163 6500 0000 0000 0048"            /* ..Replace......H */
	$"0009 005A 0073 050B 5768 6F6C 6520 776F"            /* .Æ.Z.s..Whole wo */
	$"7264 7300 0000 0000 0048 0074 005A 00D9"            /* rds......H.t.Z.Ù */
	$"050B 4967 6E6F 7265 2063 6173 6500 0000"            /* ..Ignore case... */
	$"0000 0048 00D9 005A 0140 050B 5772 6170"            /* ...H.Ù.Z.@..Wrap */
	$"2061 726F 756E 6400 0000 0000 005C 0009"            /*  around......\.Æ */
	$"006E 00D9 0517 5573 6520 5265 6775 6C61"            /* .n.Ù..Use Regula */
	$"7220 4578 7072 6573 7369 6F6E 7300"                 /* r Expressions. */
};

data 'DITL' (1024) {
	$"FFFF"                                               /* ÿÿ */
};

data 'DITL' (1025) {
	$"FFFF"                                               /* ÿÿ */
};

data 'DITL' (1026) {
	$"FFFF"                                               /* ÿÿ */
};

data 'DITL' (1027) {
	$"FFFF"                                               /* ÿÿ */
};

data 'dlgx' (518, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (128, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (255, purgeable) {
	$"0000 0000 000F"                                     /* ...... */
};

data 'dlgx' (256, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (257, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (258, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (267, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (269, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (515, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (516, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (517, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (25000, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (262, purgeable) {
	$"0000 0000 000F"                                     /* ...... */
};

data 'dlgx' (263, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (264, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (265, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (266, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (260, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (261, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (254, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (1024, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (1025, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (1026, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'dlgx' (1027, purgeable) {
	$"0000 0000 0009"                                     /* .....Æ */
};

data 'TMPL' (300, "dlgx", purgeable) {
	$"0756 6572 7369 6F6E 4457 5244 0852 6573"            /* .VersionDWRD.Res */
	$"6572 7665 6444 5752 4408 5265 7365 7276"            /* ervedDWRD.Reserv */
	$"6564 4442 5954 0852 6573 6572 7665 6442"            /* edDBYT.ReservedB */
	$"4249 5408 5265 7365 7276 6564 4242 4954"            /* BIT.ReservedBBIT */
	$"0852 6573 6572 7665 6442 4249 5408 5265"            /* .ReservedBBIT.Re */
	$"7365 7276 6564 4242 4954 1255 7365 2054"            /* servedBBIT.Use T */
	$"6865 6D65 2043 6F6E 7472 6F6C 7342 4249"            /* heme ControlsBBI */
	$"5414 4861 6E64 6C65 204D 6F76 6162 6C65"            /* T.Handle Movable */
	$"204D 6F64 616C 4242 4954 1555 7365 2043"            /*  ModalBBIT.Use C */
	$"6F6E 7472 6F6C 2048 6965 7261 7263 6879"            /* ontrol Hierarchy */
	$"4242 4954 1455 7365 2054 6865 6D65 2042"            /* BBIT.Use Theme B */
	$"6163 6B67 726F 756E 6442 4249 54"                   /* ackgroundBBIT */
};

