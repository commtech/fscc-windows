/*
    Copyright (C) 2018  Commtech, Inc.

    This file is part of fscc-windows.

    fscc-windows is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    fscc-windows is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with fscc-windows.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef FSCC_DESCRIPTOR
#define FSCC_DESCRIPTOR

struct fscc_descriptor {
    UINT32 control;
    UINT32 data_address;
    UINT32 data_count;
    UINT32 next_descriptor;
	UINT32 virtual_address; // This is for the new CommonBuffer style, where both logical and virtual address are required.
	// The above addition should be okay, as it goes past the descriptor rather than
	// being shoved somewhere in the middle.
};

#endif