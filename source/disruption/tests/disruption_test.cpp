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

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE test_disruption
#include <boost/test/unit_test.hpp>
#include "disruption/disruption_api.h"
#include "routing/raptor.h"
#include "ed/build_helper.h"
#include <boost/make_shared.hpp>
#include "tests/utils_test.h"
#include "type/chaos.pb.h"
#include "type/datetime.h"

/*

publicationDate    |------------------------------------------------------------------------------------------------|
ApplicationDate                            |--------------------------------------------|

Test1       * 20140101T0900
Test2                   * 20140103T0900
Test3                                           *20140113T0900
Test4                                                                                               *20140203T0900
Test5                                                                                                                   *20140212T0900


start_publication_date  = 2014-01-02 08:32:00
end_publication_date    = 2014-02-10 12:32:00

start_application_date  = 2014-01-12 08:32:00
end_application_date    = 2014-02-02 18:32:00

start_application_daily_hour = 08h40
end_application_daily_hour = 18h00
active_days = 11111111
*/
struct logger_initialized {
    logger_initialized()   { init_logger(); }
};
BOOST_GLOBAL_FIXTURE( logger_initialized )

namespace pt = boost::posix_time;
using navitia::type::new_disruption::Impact;
using navitia::type::new_disruption::PtObj;
using navitia::type::new_disruption::Disruption;
using navitia::type::new_disruption::Severity;

inline u_int64_t operator"" _dt_time_stamp(const char* str, size_t s) {
    return navitia::to_posix_timestamp(boost::posix_time::from_iso_string(std::string(str, s)));
}

struct DisruptionCreator {
    std::string uri;
    std::string object_uri;
    chaos::PtObject::Type object_type;
    pt::time_period application_period {pt::time_from_string("2013-12-19 12:32:00"),
                pt::time_from_string("2013-12-21 12:32:00")};
    pt::time_period publication_period {pt::time_from_string("2013-12-19 12:32:00"),
                pt::time_from_string("2013-12-21 12:32:00")};
    std::bitset<7> active_days = std::bitset<7>("1111111");
    pt::time_duration application_daily_start_hour = pt::duration_from_string("00:00");
    pt::time_duration application_daily_end_hour = pt::duration_from_string("24:00");

    std::vector<pt::time_period> get_application_periods() const {
        return navitia::expand_calendar(application_period.begin(), application_period.last(),
                                     application_daily_start_hour, application_daily_end_hour, active_days);
    }
};

class Params {
public:
    std::vector<std::string> forbidden;
    ed::builder b;
    u_int64_t end_date;

    void add_disruption(DisruptionCreator disrupt, nt::PT_Data& pt_data) {
        nt::new_disruption::DisruptionHolder& holder = pt_data.disruption_holder;

        auto disruption = std::make_unique<Disruption>();
        disruption->uri = disrupt.uri;
        disruption->publication_period = disrupt.publication_period;

        auto impact = boost::make_shared<Impact>();
        impact->uri = disrupt.uri;
        impact->application_periods = disrupt.get_application_periods();

        auto severity = boost::make_shared<Severity>();
        severity->uri = "info";
        severity->wording = "information severity";
        impact->severity = severity;

        switch (disrupt.object_type) {
        case chaos::PtObject::Type::PtObject_Type_network:
            impact->informed_entities.push_back(make_pt_obj(nt::Type_e::Network, disrupt.object_uri, pt_data, impact));
            break;
        case chaos::PtObject::Type::PtObject_Type_stop_area:
            impact->informed_entities.push_back(make_pt_obj(nt::Type_e::StopArea, disrupt.object_uri, pt_data, impact));
            break;
        case chaos::PtObject::Type::PtObject_Type_line:
            impact->informed_entities.push_back(make_pt_obj(nt::Type_e::Line, disrupt.object_uri, pt_data, impact));
            break;
        case chaos::PtObject::Type::PtObject_Type_route:
            impact->informed_entities.push_back(make_pt_obj(nt::Type_e::Route, disrupt.object_uri, pt_data, impact));
            break;
        case chaos::PtObject::Type::PtObject_Type_line_section:
            throw navitia::exception("LineSection not handled yet");
        default:
            throw navitia::exception("not handled yet");
        }

        disruption->add_impact(impact);

        holder.disruptions.push_back(std::move(disruption));
    }

    Params(): b("20120614"), end_date("20130614T000000"_dt_time_stamp) {
        std::vector<std::string> forbidden;
        b.vj_with_network("network:R", "line:A", "11111111", "", true, "")("stop_area:stop1", 8*3600 +10*60, 8*3600 + 11 * 60)
                ("stop_area:stop2", 8*3600 + 20 * 60 ,8*3600 + 21*60);
        b.vj_with_network("network:R", "line:S", "11111111", "", true, "")("stop_area:stop5", 8*3600 +10*60, 8*3600 + 11 * 60)
                ("stop_area:stop6", 8*3600 + 20 * 60 ,8*3600 + 21*60);
        b.vj_with_network("network:K","line:B","11111111","",true, "")("stop_area:stop3", 8*3600 +10*60, 8*3600 + 11 * 60)
                ("stop_area:stop4", 8*3600 + 20 * 60 ,8*3600 + 21*60);
        b.vj_with_network("network:M","line:M","11111111","",true, "")("stop_area:stop22", 8*3600 +10*60, 8*3600 + 11 * 60)
                ("stop_area:stop22", 8*3600 + 20 * 60 ,8*3600 + 21*60);
        b.vj_with_network("network:Test","line:test","11111111","",true, "")("stop_area:stop22", 8*3600 +10*60, 8*3600 + 11 * 60)
                ("stop_area:stop22", 8*3600 + 20 * 60 ,8*3600 + 21*60);
        b.generate_dummy_basis();
        b.finish();
        b.data->pt_data->index();
        b.data->build_uri();
        for(navitia::type::Line *line : b.data->pt_data->lines){
            line->network->line_list.push_back(line);
        }
        DisruptionCreator disruption_wrapper;
        disruption_wrapper = DisruptionCreator();
        disruption_wrapper.uri = "mess1";
        disruption_wrapper.object_uri = "line:A";
        disruption_wrapper.object_type = chaos::PtObject::Type::PtObject_Type_line;
        //note we add one seconds, because the end is not is the period
        disruption_wrapper.application_period = pt::time_period(pt::time_from_string("2013-12-20 12:32:00"),
                                                                pt::time_from_string("2013-12-21 12:32:00") + pt::seconds(1));
        disruption_wrapper.publication_period = pt::time_period(pt::time_from_string("2013-12-19 12:32:00"),
                                                                pt::time_from_string("2013-12-21 12:32:00") + pt::seconds(1));
        add_disruption(disruption_wrapper, *b.data->pt_data);

        disruption_wrapper = DisruptionCreator();
        disruption_wrapper.uri = "mess0";
        disruption_wrapper.object_uri = "line:S";
        disruption_wrapper.object_type = chaos::PtObject::Type::PtObject_Type_line;
        disruption_wrapper.application_period = pt::time_period(pt::time_from_string("2013-12-21 08:32:00"),
                                                                pt::time_from_string("2013-12-21 12:32:00") + pt::seconds(1));
        disruption_wrapper.publication_period = pt::time_period(pt::time_from_string("2013-12-21 08:32:00"),
                                                                pt::time_from_string("2013-12-21 12:32:00") + pt::seconds(1));
        add_disruption(disruption_wrapper, *b.data->pt_data);

        disruption_wrapper = DisruptionCreator();
        disruption_wrapper.uri = "mess2";
        disruption_wrapper.object_uri = "network:K";
        disruption_wrapper.object_type = chaos::PtObject::Type::PtObject_Type_network;
        disruption_wrapper.application_period = pt::time_period(pt::time_from_string("2013-12-23 12:32:00"),
                                                                pt::time_from_string("2013-12-25 12:32:00") + pt::seconds(1));
        disruption_wrapper.publication_period = pt::time_period(pt::time_from_string("2013-12-24 12:32:00"),
                                                                pt::time_from_string("2013-12-26 12:32:00") + pt::seconds(1));
        add_disruption(disruption_wrapper, *b.data->pt_data);
    }
};

BOOST_FIXTURE_TEST_CASE(network_filter1, Params) {
    std::vector<std::string> forbidden_uris;
    auto dt = "20131219T155000"_dt_time_stamp;
    pbnavitia::Response resp = navitia::disruption::disruptions(*(b.data),
            dt, 1, 10, 0, "network.uri=network:R", forbidden_uris);

    BOOST_REQUIRE_EQUAL(resp.disruptions_size(), 1);

    pbnavitia::Disruptions disruptions = resp.disruptions(0);
    BOOST_REQUIRE_EQUAL(disruptions.lines_size(), 1);
    BOOST_CHECK_EQUAL(disruptions.network().uri(), "network:R");

    pbnavitia::Line line = disruptions.lines(0);
    BOOST_REQUIRE_EQUAL(line.uri(), "line:A");

    BOOST_REQUIRE_EQUAL(line.disruptions_size(), 1);
    auto disruption = line.disruptions(0);
    BOOST_CHECK_EQUAL(disruption.uri(), "mess1");
    //we are in the publication period of the disruption but the application_period of the impact is in the future
    BOOST_CHECK_EQUAL(disruption.status(), pbnavitia::ActiveStatus::future);

    BOOST_REQUIRE_EQUAL(disruption.application_periods_size(), 1);
    BOOST_CHECK_EQUAL(disruption.application_periods(0).begin(), navitia::test::to_posix_timestamp("20131220T123200"));
    BOOST_CHECK_EQUAL(disruption.application_periods(0).end(), navitia::test::to_posix_timestamp("20131221T123200"));
}

BOOST_FIXTURE_TEST_CASE(network_filter2, Params) {
    std::vector<std::string> forbidden_uris;
    auto dt = "20131224T125000"_dt_time_stamp;
    pbnavitia::Response resp = navitia::disruption::disruptions(*(b.data),
            dt, 1, 10, 0, "network.uri=network:K", forbidden_uris);

    BOOST_REQUIRE_EQUAL(resp.disruptions_size(), 1);

    pbnavitia::Disruptions disruptions = resp.disruptions(0);
    BOOST_REQUIRE_EQUAL(disruptions.lines_size(), 0);
    BOOST_REQUIRE_EQUAL(disruptions.network().uri(), "network:K");
    pbnavitia::Network network = disruptions.network();
    BOOST_REQUIRE_EQUAL(network.disruptions_size(), 1);
    auto disruption = network.disruptions(0);
    BOOST_REQUIRE_EQUAL(disruption.uri(), "mess2");
    BOOST_CHECK_EQUAL(disruption.status(), pbnavitia::ActiveStatus::active);
    BOOST_REQUIRE_EQUAL(disruption.application_periods_size(), 1);
    BOOST_CHECK_EQUAL(disruption.application_periods(0).begin(), navitia::test::to_posix_timestamp("20131223T123200"));
    BOOST_CHECK_EQUAL(disruption.application_periods(0).end(), navitia::test::to_posix_timestamp("20131225T123200"));
}

BOOST_FIXTURE_TEST_CASE(line_filter, Params) {
    std::vector<std::string> forbidden_uris;
    auto dt = "20131221T085000"_dt_time_stamp;
    pbnavitia::Response resp = navitia::disruption::disruptions(*(b.data),
            dt, 1 ,10 ,0 , "line.uri=line:S", forbidden_uris);

    BOOST_REQUIRE_EQUAL(resp.disruptions_size(), 1);

    pbnavitia::Disruptions disruptions = resp.disruptions(0);
    BOOST_REQUIRE_EQUAL(disruptions.lines_size(), 1);
    BOOST_REQUIRE_EQUAL(disruptions.network().uri(), "network:R");

    pbnavitia::Line line = disruptions.lines(0);
    BOOST_REQUIRE_EQUAL(line.uri(), "line:S");

    BOOST_REQUIRE_EQUAL(line.disruptions_size(), 1);
    auto disruption = line.disruptions(0);
    BOOST_REQUIRE_EQUAL(disruption.uri(), "mess0");
    BOOST_CHECK_EQUAL(disruption.status(), pbnavitia::ActiveStatus::active);

    BOOST_REQUIRE_EQUAL(disruption.application_periods_size(), 1);
    BOOST_CHECK_EQUAL(disruption.application_periods(0).begin(), navitia::test::to_posix_timestamp("20131221T083200"));
    BOOST_CHECK_EQUAL(disruption.application_periods(0).end(), navitia::test::to_posix_timestamp("20131221T123200"));
}

BOOST_FIXTURE_TEST_CASE(Test1, Params) {
    std::vector<std::string> forbidden_uris;
    auto dt = "20140101T0900"_dt_time_stamp;
    pbnavitia::Response resp = navitia::disruption::disruptions(*(b.data),
            dt, 1, 10, 0, "", forbidden_uris);
    BOOST_REQUIRE_EQUAL(resp.response_type(), pbnavitia::ResponseType::NO_SOLUTION);
}

BOOST_FIXTURE_TEST_CASE(Test2, Params) {
    std::vector<std::string> forbidden_uris;
    auto dt = "20131226T0900"_dt_time_stamp;
    pbnavitia::Response resp = navitia::disruption::disruptions(*(b.data),
            dt, 1, 10, 0, "", forbidden_uris);
    BOOST_REQUIRE_EQUAL(resp.disruptions_size(), 1);

    pbnavitia::Disruptions disruptions = resp.disruptions(0);
    BOOST_REQUIRE_EQUAL(disruptions.lines_size(), 0);
    BOOST_REQUIRE_EQUAL(disruptions.network().uri(), "network:K");

    BOOST_REQUIRE_EQUAL(disruptions.network().disruptions_size(), 1);

    auto disruption = disruptions.network().disruptions(0);
    BOOST_REQUIRE_EQUAL(disruption.uri(), "mess2");
    BOOST_CHECK_EQUAL(disruption.status(), pbnavitia::ActiveStatus::past);
}

BOOST_FIXTURE_TEST_CASE(Test4, Params) {
    std::cout << "bob 4?" << std::endl;
    std::vector<std::string> forbidden_uris;
    auto dt = "20130203T0900"_dt_time_stamp;
    pbnavitia::Response resp = navitia::disruption::disruptions(*(b.data),
            dt, 1 , 10, 0, "", forbidden_uris);
    BOOST_REQUIRE_EQUAL(resp.response_type(), pbnavitia::ResponseType::NO_SOLUTION);
}

BOOST_FIXTURE_TEST_CASE(Test5, Params) {
    std::vector<std::string> forbidden_uris;
    auto dt = "20130212T0900"_dt_time_stamp;
    pbnavitia::Response resp = navitia::disruption::disruptions(*(b.data),
            dt, 1, 10, 0, "", forbidden_uris);

    BOOST_REQUIRE_EQUAL(resp.response_type(), pbnavitia::ResponseType::NO_SOLUTION);
}

