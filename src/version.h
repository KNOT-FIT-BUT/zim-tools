/*
 * Copyright (C) Emmanuel Engelhart <kellson@kiwix.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef _ZIM_TOOL_VERSION_H_
#define _ZIM_TOOL_VERSION_H_

#ifndef VERSION
  #define VERSION "undefined"
#endif

void version()
{
  std::cout << VERSION << std::endl;
}

#endif //_ZIM_TOOL_VERSION_H_
