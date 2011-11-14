/*
 * Copyright (C) 2011 by the Widelands Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef LOGIC_WAREWORKER_H
#define LOGIC_WAREWORKER_H

namespace Widelands {

/**
 * For @ref Request, @ref WaresDisplay et al., indicate whether the object
 * or object type in question is a ware or a worker.
 *
 * @note These values are written into savegames
 */
enum WareWorker {
	wwWARE = 0,
	wwWORKER = 1
};

} // namespace Widelands

#endif // LOGIC_WAREWORKER_H
