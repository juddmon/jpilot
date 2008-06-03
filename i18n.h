/* $Id: i18n.h,v 1.9 2008/06/03 19:15:46 rousseau Exp $ */

/*******************************************************************************
 * i18n.h
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2002 by Judd Montgomery
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ******************************************************************************/

#ifndef __I18N_H__
#  define __I18N_H__

#  include "config.h"

#  if defined(HAVE_GETTEXT)
#    include <locale.h>
#    include "gettext.h"
#    define _(str) gettext(str)
#    define N_(str) str
#    define C_(context, str) pgettext(context, str)
#  else
#    define _(str) str
#    define N_(str) str
#    define C_(context, str) str
#  endif

#endif
