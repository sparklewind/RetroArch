/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gl_font.h"

void gl_init_font(gl_t *gl, const char *font_path, unsigned font_size)
{
   gl_init_font_ctx(gl, font_path, font_size);
}

void gl_deinit_font(gl_t *gl)
{
   gl_deinit_font_ctx(gl);
}


void gl_render_msg(gl_t *gl, const char *msg)
{
   gl_render_msg_ctx(gl, msg);
}