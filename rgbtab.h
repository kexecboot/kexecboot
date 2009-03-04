/*
 * Copyright (c) 2008-2009 Yuri Bushmelev <jay4mail@gmail.com>
 * Copyright (C) 1989-95 GROUPE BULL
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * GROUPE BULL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of GROUPE BULL shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from GROUPE BULL.
 */

/*****************************************************************************\
* rgbtab.h                                                                    *
*                                                                             *
* A hard coded rgb.txt. To keep it short I removed all colornames with        *
* trailing numbers, Blue3 etc, except the GrayXX. Sorry Grey-lovers I prefer  *
* Gray ;-). But Grey is recognized on lookups, only on save Gray will be      *
* used, maybe you want to do some substitue there too.                        *
*                                                                             *
* To save memory the RGBs are coded in one long value, as done by the RGB     *
* macro.                                                                      *
*                                                                             *
* Developed by HeDu 3/94 (hedu@cul-ipn.uni-kiel.de)                           *
\*****************************************************************************/

#ifndef _HAVE_RGBTAB_H
#define _HAVE_RGBTAB_H

#include <stdint.h>

struct xpm_named_color_t {
	char *name;
//     uint32_t rgb;
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

/*
#define MYRGB(r,g,b) \
	((uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b)
*/

#define MYRGB(r,g,b) (r), (g), (b)

/* NOTE: Keep array sorted by name! Binary search is used. */
struct xpm_named_color_t xpm_color_names[] = {
    {"aliceblue", MYRGB(240, 248, 255)},
    {"antiquewhite", MYRGB(250, 235, 215)},
    {"aquamarine", MYRGB(50, 191, 193)},
    {"azure", MYRGB(240, 255, 255)},
    {"beige", MYRGB(245, 245, 220)},
    {"bisque", MYRGB(255, 228, 196)},
    {"black", MYRGB(0, 0, 0)},
    {"blanchedalmond", MYRGB(255, 235, 205)},
    {"blue", MYRGB(0, 0, 255)},
    {"blueviolet", MYRGB(138, 43, 226)},
    {"brown", MYRGB(165, 42, 42)},
    {"burlywood", MYRGB(222, 184, 135)},
    {"cadetblue", MYRGB(95, 146, 158)},
    {"chartreuse", MYRGB(127, 255, 0)},
    {"chocolate", MYRGB(210, 105, 30)},
    {"coral", MYRGB(255, 114, 86)},
    {"cornflowerblue", MYRGB(34, 34, 152)},
    {"cornsilk", MYRGB(255, 248, 220)},
    {"cyan", MYRGB(0, 255, 255)},
    {"darkgoldenrod", MYRGB(184, 134, 11)},
    {"darkgreen", MYRGB(0, 86, 45)},
    {"darkkhaki", MYRGB(189, 183, 107)},
    {"darkolivegreen", MYRGB(85, 86, 47)},
    {"darkorange", MYRGB(255, 140, 0)},
    {"darkorchid", MYRGB(139, 32, 139)},
    {"darksalmon", MYRGB(233, 150, 122)},
    {"darkseagreen", MYRGB(143, 188, 143)},
    {"darkslateblue", MYRGB(56, 75, 102)},
    {"darkslategray", MYRGB(47, 79, 79)},
    {"darkturquoise", MYRGB(0, 166, 166)},
    {"darkviolet", MYRGB(148, 0, 211)},
    {"deeppink", MYRGB(255, 20, 147)},
    {"deepskyblue", MYRGB(0, 191, 255)},
    {"dimgray", MYRGB(84, 84, 84)},
    {"dodgerblue", MYRGB(30, 144, 255)},
    {"firebrick", MYRGB(142, 35, 35)},
    {"floralwhite", MYRGB(255, 250, 240)},
    {"forestgreen", MYRGB(80, 159, 105)},
    {"gainsboro", MYRGB(220, 220, 220)},
    {"ghostwhite", MYRGB(248, 248, 255)},
    {"gold", MYRGB(218, 170, 0)},
    {"goldenrod", MYRGB(239, 223, 132)},
    {"gray", MYRGB(126, 126, 126)},
    {"gray0", MYRGB(0, 0, 0)},
    {"gray1", MYRGB(3, 3, 3)},
    {"gray10", MYRGB(26, 26, 26)},
    {"gray100", MYRGB(255, 255, 255)},
    {"gray11", MYRGB(28, 28, 28)},
    {"gray12", MYRGB(31, 31, 31)},
    {"gray13", MYRGB(33, 33, 33)},
    {"gray14", MYRGB(36, 36, 36)},
    {"gray15", MYRGB(38, 38, 38)},
    {"gray16", MYRGB(41, 41, 41)},
    {"gray17", MYRGB(43, 43, 43)},
    {"gray18", MYRGB(46, 46, 46)},
    {"gray19", MYRGB(48, 48, 48)},
    {"gray2", MYRGB(5, 5, 5)},
    {"gray20", MYRGB(51, 51, 51)},
    {"gray21", MYRGB(54, 54, 54)},
    {"gray22", MYRGB(56, 56, 56)},
    {"gray23", MYRGB(59, 59, 59)},
    {"gray24", MYRGB(61, 61, 61)},
    {"gray25", MYRGB(64, 64, 64)},
    {"gray26", MYRGB(66, 66, 66)},
    {"gray27", MYRGB(69, 69, 69)},
    {"gray28", MYRGB(71, 71, 71)},
    {"gray29", MYRGB(74, 74, 74)},
    {"gray3", MYRGB(8, 8, 8)},
    {"gray30", MYRGB(77, 77, 77)},
    {"gray31", MYRGB(79, 79, 79)},
    {"gray32", MYRGB(82, 82, 82)},
    {"gray33", MYRGB(84, 84, 84)},
    {"gray34", MYRGB(87, 87, 87)},
    {"gray35", MYRGB(89, 89, 89)},
    {"gray36", MYRGB(92, 92, 92)},
    {"gray37", MYRGB(94, 94, 94)},
    {"gray38", MYRGB(97, 97, 97)},
    {"gray39", MYRGB(99, 99, 99)},
    {"gray4", MYRGB(10, 10, 10)},
    {"gray40", MYRGB(102, 102, 102)},
    {"gray41", MYRGB(105, 105, 105)},
    {"gray42", MYRGB(107, 107, 107)},
    {"gray43", MYRGB(110, 110, 110)},
    {"gray44", MYRGB(112, 112, 112)},
    {"gray45", MYRGB(115, 115, 115)},
    {"gray46", MYRGB(117, 117, 117)},
    {"gray47", MYRGB(120, 120, 120)},
    {"gray48", MYRGB(122, 122, 122)},
    {"gray49", MYRGB(125, 125, 125)},
    {"gray5", MYRGB(13, 13, 13)},
    {"gray50", MYRGB(127, 127, 127)},
    {"gray51", MYRGB(130, 130, 130)},
    {"gray52", MYRGB(133, 133, 133)},
    {"gray53", MYRGB(135, 135, 135)},
    {"gray54", MYRGB(138, 138, 138)},
    {"gray55", MYRGB(140, 140, 140)},
    {"gray56", MYRGB(143, 143, 143)},
    {"gray57", MYRGB(145, 145, 145)},
    {"gray58", MYRGB(148, 148, 148)},
    {"gray59", MYRGB(150, 150, 150)},
    {"gray6", MYRGB(15, 15, 15)},
    {"gray60", MYRGB(153, 153, 153)},
    {"gray61", MYRGB(156, 156, 156)},
    {"gray62", MYRGB(158, 158, 158)},
    {"gray63", MYRGB(161, 161, 161)},
    {"gray64", MYRGB(163, 163, 163)},
    {"gray65", MYRGB(166, 166, 166)},
    {"gray66", MYRGB(168, 168, 168)},
    {"gray67", MYRGB(171, 171, 171)},
    {"gray68", MYRGB(173, 173, 173)},
    {"gray69", MYRGB(176, 176, 176)},
    {"gray7", MYRGB(18, 18, 18)},
    {"gray70", MYRGB(179, 179, 179)},
    {"gray71", MYRGB(181, 181, 181)},
    {"gray72", MYRGB(184, 184, 184)},
    {"gray73", MYRGB(186, 186, 186)},
    {"gray74", MYRGB(189, 189, 189)},
    {"gray75", MYRGB(191, 191, 191)},
    {"gray76", MYRGB(194, 194, 194)},
    {"gray77", MYRGB(196, 196, 196)},
    {"gray78", MYRGB(199, 199, 199)},
    {"gray79", MYRGB(201, 201, 201)},
    {"gray8", MYRGB(20, 20, 20)},
    {"gray80", MYRGB(204, 204, 204)},
    {"gray81", MYRGB(207, 207, 207)},
    {"gray82", MYRGB(209, 209, 209)},
    {"gray83", MYRGB(212, 212, 212)},
    {"gray84", MYRGB(214, 214, 214)},
    {"gray85", MYRGB(217, 217, 217)},
    {"gray86", MYRGB(219, 219, 219)},
    {"gray87", MYRGB(222, 222, 222)},
    {"gray88", MYRGB(224, 224, 224)},
    {"gray89", MYRGB(227, 227, 227)},
    {"gray9", MYRGB(23, 23, 23)},
    {"gray90", MYRGB(229, 229, 229)},
    {"gray91", MYRGB(232, 232, 232)},
    {"gray92", MYRGB(235, 235, 235)},
    {"gray93", MYRGB(237, 237, 237)},
    {"gray94", MYRGB(240, 240, 240)},
    {"gray95", MYRGB(242, 242, 242)},
    {"gray96", MYRGB(245, 245, 245)},
    {"gray97", MYRGB(247, 247, 247)},
    {"gray98", MYRGB(250, 250, 250)},
    {"gray99", MYRGB(252, 252, 252)},
    {"green", MYRGB(0, 255, 0)},
    {"greenyellow", MYRGB(173, 255, 47)},
    {"honeydew", MYRGB(240, 255, 240)},
    {"hotpink", MYRGB(255, 105, 180)},
    {"indianred", MYRGB(107, 57, 57)},
    {"ivory", MYRGB(255, 255, 240)},
    {"khaki", MYRGB(179, 179, 126)},
    {"lavender", MYRGB(230, 230, 250)},
    {"lavenderblush", MYRGB(255, 240, 245)},
    {"lawngreen", MYRGB(124, 252, 0)},
    {"lemonchiffon", MYRGB(255, 250, 205)},
    {"lightblue", MYRGB(176, 226, 255)},
    {"lightcoral", MYRGB(240, 128, 128)},
    {"lightcyan", MYRGB(224, 255, 255)},
    {"lightgoldenrod", MYRGB(238, 221, 130)},
    {"lightgoldenrodyellow", MYRGB(250, 250, 210)},
    {"lightgray", MYRGB(168, 168, 168)},
    {"lightpink", MYRGB(255, 182, 193)},
    {"lightsalmon", MYRGB(255, 160, 122)},
    {"lightseagreen", MYRGB(32, 178, 170)},
    {"lightskyblue", MYRGB(135, 206, 250)},
    {"lightslateblue", MYRGB(132, 112, 255)},
    {"lightslategray", MYRGB(119, 136, 153)},
    {"lightsteelblue", MYRGB(124, 152, 211)},
    {"lightyellow", MYRGB(255, 255, 224)},
    {"limegreen", MYRGB(0, 175, 20)},
    {"linen", MYRGB(250, 240, 230)},
    {"magenta", MYRGB(255, 0, 255)},
    {"maroon", MYRGB(143, 0, 82)},
    {"mediumaquamarine", MYRGB(0, 147, 143)},
    {"mediumblue", MYRGB(50, 50, 204)},
    {"mediumforestgreen", MYRGB(50, 129, 75)},
    {"mediumgoldenrod", MYRGB(209, 193, 102)},
    {"mediumorchid", MYRGB(189, 82, 189)},
    {"mediumpurple", MYRGB(147, 112, 219)},
    {"mediumseagreen", MYRGB(52, 119, 102)},
    {"mediumslateblue", MYRGB(106, 106, 141)},
    {"mediumspringgreen", MYRGB(35, 142, 35)},
    {"mediumturquoise", MYRGB(0, 210, 210)},
    {"mediumvioletred", MYRGB(213, 32, 121)},
    {"midnightblue", MYRGB(47, 47, 100)},
    {"mintcream", MYRGB(245, 255, 250)},
    {"mistyrose", MYRGB(255, 228, 225)},
    {"moccasin", MYRGB(255, 228, 181)},
    {"navajowhite", MYRGB(255, 222, 173)},
    {"navy", MYRGB(35, 35, 117)},
    {"navyblue", MYRGB(35, 35, 117)},
    {"oldlace", MYRGB(253, 245, 230)},
    {"olivedrab", MYRGB(107, 142, 35)},
    {"orange", MYRGB(255, 135, 0)},
    {"orangered", MYRGB(255, 69, 0)},
    {"orchid", MYRGB(239, 132, 239)},
    {"palegoldenrod", MYRGB(238, 232, 170)},
    {"palegreen", MYRGB(115, 222, 120)},
    {"paleturquoise", MYRGB(175, 238, 238)},
    {"palevioletred", MYRGB(219, 112, 147)},
    {"papayawhip", MYRGB(255, 239, 213)},
    {"peachpuff", MYRGB(255, 218, 185)},
    {"peru", MYRGB(205, 133, 63)},
    {"pink", MYRGB(255, 181, 197)},
    {"plum", MYRGB(197, 72, 155)},
    {"powderblue", MYRGB(176, 224, 230)},
    {"purple", MYRGB(160, 32, 240)},
    {"red", MYRGB(255, 0, 0)},
    {"rosybrown", MYRGB(188, 143, 143)},
    {"royalblue", MYRGB(65, 105, 225)},
    {"saddlebrown", MYRGB(139, 69, 19)},
    {"salmon", MYRGB(233, 150, 122)},
    {"sandybrown", MYRGB(244, 164, 96)},
    {"seagreen", MYRGB(82, 149, 132)},
    {"seashell", MYRGB(255, 245, 238)},
    {"sienna", MYRGB(150, 82, 45)},
    {"skyblue", MYRGB(114, 159, 255)},
    {"slateblue", MYRGB(126, 136, 171)},
    {"slategray", MYRGB(112, 128, 144)},
    {"snow", MYRGB(255, 250, 250)},
    {"springgreen", MYRGB(65, 172, 65)},
    {"steelblue", MYRGB(84, 112, 170)},
    {"tan", MYRGB(222, 184, 135)},
    {"thistle", MYRGB(216, 191, 216)},
    {"tomato", MYRGB(255, 99, 71)},
    {"transparent", MYRGB(0, 0, 1)},
    {"turquoise", MYRGB(25, 204, 223)},
    {"violet", MYRGB(156, 62, 206)},
    {"violetred", MYRGB(243, 62, 150)},
    {"wheat", MYRGB(245, 222, 179)},
    {"white", MYRGB(255, 255, 255)},
    {"whitesmoke", MYRGB(245, 245, 245)},
    {"yellow", MYRGB(255, 255, 0)},
    {"yellowgreen", MYRGB(50, 216, 56)},
    {NULL, 0}
};

#endif	/* RGBTAB_H */
