/* i18n.h
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2001 by Judd Montgomery
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
 */
#ifndef __I18N_H__
#define __I18N_H__

#include "config.h"

#if defined(ENABLE_NLS)
#if defined(HAVE_GETTEXT)
#include	<libintl.h>
#else
#include	"intl/libintl.h"
#endif
#define	_(str)	gettext(str)
#else
#define	_(str)	str
#endif

#ifndef gettext_noop
#define	gettext_noop(str) str
#endif
#ifndef N_
#define N_(str) str
#endif

#endif
