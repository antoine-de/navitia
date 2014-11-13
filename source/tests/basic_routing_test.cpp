/* Copyright © 2001-2014, Canal TP and/or its affiliates. All rights reserved.

This file is part of Navitia,
    the software to build cool stuff with public transport.

Hope you'll enjoy and contribute to this project,
    powered by Canal TP (www.canaltp.fr).
Help us simplify mobility and open public transport:
    a non ending quest to the responsive locomotion way of traveling!

LICENCE: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Stay tuned using
twitter @navitia
IRC #navitia on freenode
https://groups.google.com/d/forum/navitia
www.navitia.io
*/

#include "utils/init.h"
#include "routing/tests/routing_api_test_data.h"
#include "mock_kraken.h"

/*
 *               2 minutes connection
 *    A       B----------------------->C        D
 *    8h     8h05                     14h07     15h
 *    15    15h05                     15h10     15h20
 *    15h                                       15h10   (only day 2)
 *
 * Two long waiting time
 *            2 minutes connection
 *    E      F----------------------->G         H
 *    8h    9h                        17h       18h
 */

int main(int argc, const char* const argv[]) {
    navitia::init_app();

    ed::builder b = {"20120614"};
    b.generate_dummy_basis();
    b.vj("l1")("A", 8*3600)("B", 8*3600+5*60);
    b.vj("l2")("A", 15*3600)("B", 15*3600+5*60);
    b.vj("l3")("C", 14*3600+7*60)("D", 15*3600);
    b.vj("l4")("C", 15*3600+10*60)("D", 16*3600);
    b.vj("l5","10", "", true)("A", 15*3600)("D", 15*3600+10*60);
    b.vj("l6")("E", 8*3600)("F", 9*3600);
    b.vj("l7")("G", 17*3600)("H", 18*3600);
    b.connection("B", "C", 2*60);
    b.connection("F", "G", 2*60);
    b.data->pt_data->index();
    b.data->build_raptor();
    b.data->build_uri();

    mock_kraken kraken(b, "basic_routing_test", argc, argv);
    return 0;
}

