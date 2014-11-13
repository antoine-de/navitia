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
#include "type/type.h"

using namespace navitia::georef;
/*
 * Here we test prepare a data for autocomplete test
 * The data contains One Admin, 6 stop_areas and three addresses
 */
int main(int argc, const char* const argv[]) {
    navitia::init_app();

    ed::builder b = {"20140614"};

    b.sa("IUT", 0, 0);
    b.sa("Gare", 0, 0);
    b.sa("Becharles", 0, 0);
    b.sa("Luther King", 0, 0);
    b.sa("Napoleon III", 0, 0);
    b.sa("MPT kerfeunteun", 0, 0);
    b.data->pt_data->index();

    Way w;
    w.idx = 0;
    w.name = "rue DU TREGOR";
    w.uri = w.name;
    b.data->geo_ref->add_way(w);
    w.name = "rue VIS";
    w.uri = w.name;
    w.idx = 1;
    b.data->geo_ref->add_way(w);
    w.idx = 2;
    w.name = "quai NEUF";
    w.uri = w.name;
    b.data->geo_ref->add_way(w);

    Admin* ad = new Admin;
    ad->name = "Quimper";
    ad->uri = "Quimper";
    ad->level = 8;
    ad->post_code = "29000";
    ad->idx = 0;
    b.data->geo_ref->admins.push_back(ad);
    b.manage_admin();
    b.build_autocomplete();


    b.data->pt_data->index();
    b.data->build_raptor();
    b.data->build_uri();

    mock_kraken kraken(b, "main_autocomplete_test", argc, argv);
    return 0;
}

