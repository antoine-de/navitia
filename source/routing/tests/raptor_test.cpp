#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE test_raptor
#include <boost/test/unit_test.hpp>

#include "routing/raptor.h"
#include "ed/build_helper.h"


using namespace navitia;
using namespace routing;
namespace bt = boost::posix_time;

BOOST_AUTO_TEST_CASE(direct){
    ed::builder b("20120614");
    b.vj("A")("stop1", 8000, 8050)("stop2", 8100,8150);
    b.data.pt_data.index();
    b.data.build_raptor();
    RAPTOR raptor(b.data);

    auto res1 = raptor.compute(b.data.pt_data.stop_areas[0], b.data.pt_data.stop_areas[1], 7900, 0, DateTimeUtils::inf, false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    auto res = res1.back();
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, 0);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[0].departure.time_of_day().total_seconds(), 8050);
    BOOST_CHECK_EQUAL(res.items[0].arrival.time_of_day().total_seconds(), 8100);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].arrival, b.data)), 0);

    res1 = raptor.compute(b.data.pt_data.stop_areas[0], b.data.pt_data.stop_areas[1], 7900, 0, DateTimeUtils::set(0, 8200), false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 1);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, 0);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[0].departure.time_of_day().total_seconds(), 8050);
    BOOST_CHECK_EQUAL(res.items[0].arrival.time_of_day().total_seconds(), 8100);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].arrival, b.data)), 0);

    res1 = raptor.compute(b.data.pt_data.stop_areas[0], b.data.pt_data.stop_areas[1], 7900, 0, DateTimeUtils::set(0, 8100), false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 1);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, 0);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[0].departure.time_of_day().total_seconds(), 8050);
    BOOST_CHECK_EQUAL(res.items[0].arrival.time_of_day().total_seconds(), 8100);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].arrival, b.data)), 0);

    res1 = raptor.compute(b.data.pt_data.stop_areas[0], b.data.pt_data.stop_areas[1], 7900, 0, DateTimeUtils::set(0, 8099), false);

    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}


BOOST_AUTO_TEST_CASE(change){
    ed::builder b("20120614");
    b.vj("A")("stop1", 8000, 8050)("stop2", 8100, 8150)("stop3", 8200, 8250);
    b.vj("B")("stop4", 8000, 8050)("stop2", 8300, 8350)("stop5", 8400, 8450);
    b.data.pt_data.index();
    b.data.build_raptor();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas[0], d.stop_areas[4], 7900, 0, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    auto res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 3);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, 0);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, 1);

    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, 1);

    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[1]->idx, 4);

    BOOST_CHECK_EQUAL(res.items[0].departure.time_of_day().total_seconds(), 8050);
    BOOST_CHECK_EQUAL(res.items[0].arrival.time_of_day().total_seconds(), 8100);

    BOOST_CHECK_EQUAL(res.items[1].departure.time_of_day().total_seconds(), 8100);
    BOOST_CHECK_EQUAL(res.items[1].arrival.time_of_day().total_seconds(), 8350);

    BOOST_CHECK_EQUAL(res.items[2].departure.time_of_day().total_seconds(), 8350);
    BOOST_CHECK_EQUAL(res.items[2].arrival.time_of_day().total_seconds(), 8400);

    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].arrival, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[1].arrival, b.data)), 0);

    res1 = raptor.compute(d.stop_areas[0], d.stop_areas[4], 7900, 0, DateTimeUtils::set(0,8500), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 3);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, 0);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[1]->idx, 4);
    BOOST_CHECK_EQUAL(res.items[0].departure.time_of_day().total_seconds(), 8050);
    BOOST_CHECK_EQUAL(res.items[0].arrival.time_of_day().total_seconds(), 8100);

    BOOST_CHECK_EQUAL(res.items[1].departure.time_of_day().total_seconds(), 8100);
    BOOST_CHECK_EQUAL(res.items[1].arrival.time_of_day().total_seconds(), 8350);

    BOOST_CHECK_EQUAL(res.items[2].departure.time_of_day().total_seconds(), 8350);
    BOOST_CHECK_EQUAL(res.items[2].arrival.time_of_day().total_seconds(), 8400);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].arrival, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[1].arrival, b.data)), 0);

    res1 = raptor.compute(d.stop_areas[0], d.stop_areas[4], 7900, 0, DateTimeUtils::set(0, 8400), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 3);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, 0);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[1]->idx, 4);
    BOOST_CHECK_EQUAL(res.items[0].departure.time_of_day().total_seconds(), 8050);
    BOOST_CHECK_EQUAL(res.items[0].arrival.time_of_day().total_seconds(), 8100);

    BOOST_CHECK_EQUAL(res.items[1].departure.time_of_day().total_seconds(), 8100);
    BOOST_CHECK_EQUAL(res.items[1].arrival.time_of_day().total_seconds(), 8350);

    BOOST_CHECK_EQUAL(res.items[2].departure.time_of_day().total_seconds(), 8350);
    BOOST_CHECK_EQUAL(res.items[2].arrival.time_of_day().total_seconds(), 8400);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].arrival, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[1].arrival, b.data)), 0);


    res1 = raptor.compute(d.stop_areas[0], d.stop_areas[4], 7900, 0, DateTimeUtils::set(0, 8399), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}


BOOST_AUTO_TEST_CASE(passe_minuit){
    ed::builder b("20120614");
    b.vj("A")("stop1", 23*3600)("stop2", 24*3600 + 5*60);
    b.vj("B")("stop2", 10*60)("stop3", 20*60);
    b.data.pt_data.index();
    b.data.build_raptor();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas[0], d.stop_areas[2], 22*3600, 0, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    auto res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 3);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, 0);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[1]->idx, 2);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[1].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas[0], d.stop_areas[2], 22*3600, 0, DateTimeUtils::set(1, 8500), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 3);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, 0);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[1]->idx, 2);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[1].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas[0], d.stop_areas[2], 22*3600, 0, DateTimeUtils::set(1, 20*60), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 3);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, 0);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, 1);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[1]->idx, 2);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[1].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas[0], d.stop_areas[2], 22*3600, 0, DateTimeUtils::set(1, (20*60)-1), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}


BOOST_AUTO_TEST_CASE(passe_minuit_2){
    ed::builder b("20120614");
    b.vj("A")("stop1", 23*3600)("stop2", 23*3600 + 59*60);
    b.vj("B")("stop4", 23*3600 + 10*60)("stop2", 10*60)("stop3", 20*60);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 22*3600, 0, DateTimeUtils::inf, false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    auto res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 3);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, d.stop_points_map["stop1"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[1]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[1].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 22*3600, 0, DateTimeUtils::set(1, 5000), false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 3);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, d.stop_points_map["stop1"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[1]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[1].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 22*3600, 0, DateTimeUtils::set(1, 20*60), false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 3);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, d.stop_points_map["stop1"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[1]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[1].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 22*3600, 0, DateTimeUtils::set(1, (20*60)-1), false);

    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}


BOOST_AUTO_TEST_CASE(passe_minuit_interne){
    ed::builder b("20120614");
    b.vj("A")("stop1", 23*3600)("stop2", 23*3600 + 30*60, 24*3600 + 30*60)("stop3", 24*3600 + 40 * 60);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 22*3600, 0, DateTimeUtils::inf, false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    auto res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 1);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, d.stop_areas_map["stop1"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, d.stop_areas_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[2]->idx, d.stop_areas_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 22*3600, 0, DateTimeUtils::set(1, 3600), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 1);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, d.stop_areas_map["stop1"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, d.stop_areas_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[2]->idx, d.stop_areas_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 22*3600, 0, DateTimeUtils::set(1, 40*60), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 1);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, d.stop_areas_map["stop1"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, d.stop_areas_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[2]->idx, d.stop_areas_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].departure, b.data)), 0);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[0].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 22*3600, 0, DateTimeUtils::set(1, (40*60)-1), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}


BOOST_AUTO_TEST_CASE(validity_pattern){
    ed::builder b("20120614");
    b.vj("D", "10", "", true)("stop1", 8000)("stop2", 8200);
    b.vj("D", "1", "", true)("stop1", 9000)("stop2", 9200);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7800, 0, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    auto res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 1);
    BOOST_CHECK_EQUAL(res.items[0].arrival.time_of_day().total_seconds(), 9200);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7800, 0, DateTimeUtils::set(0, 10000), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 1);
    BOOST_CHECK_EQUAL(res.items[0].arrival.time_of_day().total_seconds(), 9200);


    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7800, 0, DateTimeUtils::set(0, 9200), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 1);
    BOOST_CHECK_EQUAL(res.items[0].arrival.time_of_day().total_seconds(), 9200);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7800, 0, DateTimeUtils::set(0, 9199), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 0);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7900, 2, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}

/*BOOST_AUTO_TEST_CASE(validit_pattern_2) {
    ed::builder b("20120614");
    b.vj("D", "10", "", true)("stop1", 8000)("stop2", 8200);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7800, 0, DateTimeUtils::inf);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    auto res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 1);
    BOOST_CHECK_EQUAL(res.items[0].arrival.day(), 1);
}
*/

BOOST_AUTO_TEST_CASE(marche_a_pied_milieu){
    ed::builder b("20120614");
    b.vj("A", "11111111", "", true)("stop1", 8000,8050)("stop2", 8200,8250);
    b.vj("B", "11111111", "", true)("stop3", 9000,9050)("stop4", 9200,9250);
    b.connection("stop2", "stop3", 10*60);
    b.connection("stop3", "stop2", 10*60);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 7900, 0, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    auto res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 4);
    BOOST_CHECK_EQUAL(res.items[3].arrival.time_of_day().total_seconds(), 9200);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 7900, 0, DateTimeUtils::set(0,10000), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 4);
    BOOST_CHECK_EQUAL(res.items[3].arrival.time_of_day().total_seconds(), 9200);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 7900, 0, DateTimeUtils::set(0,9200), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 4);
    BOOST_CHECK_EQUAL(res.items[3].arrival.time_of_day().total_seconds(), 9200);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 7900, 0, DateTimeUtils::set(0,9199), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}

BOOST_AUTO_TEST_CASE(marche_a_pied_pam){
    ed::builder b("20120614");
    b.vj("A")("stop1", 8000)("stop2", 23*3600+59*60);
    b.vj("B")("stop3", 2*3600)("stop4",2*3600+20);
    b.connection("stop2", "stop3", 10*60);
    b.connection("stop3", "stop2", 10*60);

    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 7900, 0, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    auto res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 4);
    BOOST_CHECK_EQUAL(res.items[3].stop_points[1]->idx, d.stop_areas_map["stop4"]->idx);
    BOOST_CHECK_EQUAL(res.items[3].arrival.time_of_day().total_seconds(), 2*3600+20);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[3].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 7900, 0, DateTimeUtils::set(1 , (3*3600+20)), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 4);
    BOOST_CHECK_EQUAL(res.items[3].stop_points[1]->idx, d.stop_areas_map["stop4"]->idx);
    BOOST_CHECK_EQUAL(res.items[3].arrival.time_of_day().total_seconds(), 2*3600+20);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[3].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 7900, 0, DateTimeUtils::set(1 , (2*3600+20)), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();
    BOOST_REQUIRE_EQUAL(res.items.size(), 4);
    BOOST_CHECK_EQUAL(res.items[3].stop_points[1]->idx, d.stop_areas_map["stop4"]->idx);
    BOOST_CHECK_EQUAL(res.items[3].arrival.time_of_day().total_seconds(), 2*3600+20);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res.items[3].arrival, b.data)), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 7900, 0, DateTimeUtils::set(1 , (2*3600+20)-1), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}


BOOST_AUTO_TEST_CASE(test_rattrapage) {
    ed::builder b("20120614");
    b.vj("A")("stop1", 8*3600 + 10*60)("stop2", 8*3600 + 15*60)("stop3", 8*3600 + 35*60)("stop4", 8*3600 + 45*60);
    b.vj("B")("stop1", 8*3600 + 20*60)("stop2", 8*3600 + 25*60)("stop3", 8*3600 + 40*60)("stop4", 8*3600 + 50*60);
    b.vj("C")("stop2", 8*3600 + 30*60)("stop5", 8*3600 + 31*60)("stop3", 8*3600 + 32*60);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 8*3600 + 15*60, 0, DateTimeUtils::inf, false);


    BOOST_REQUIRE_EQUAL(res1.size(), 2);

    auto res = res1.back();


    BOOST_REQUIRE_EQUAL(res.items.size(), 5);
    BOOST_REQUIRE_EQUAL(res.items[0].stop_points.size(), 2);
    BOOST_REQUIRE_EQUAL(res.items[1].stop_points.size(), 1);
    BOOST_REQUIRE_EQUAL(res.items[2].stop_points.size(), 3);
    BOOST_REQUIRE_EQUAL(res.items[3].stop_points.size(), 1);
    BOOST_REQUIRE_EQUAL(res.items[4].stop_points.size(), 2);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, d.stop_points_map["stop1"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[2]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(res.items[3].stop_points[0]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(res.items[4].stop_points[0]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(res.items[4].stop_points[1]->idx, d.stop_points_map["stop4"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].departure.time_of_day().total_seconds(), 8*3600+20*60);
    BOOST_CHECK_EQUAL(res.items[4].arrival.time_of_day().total_seconds(), 8*3600+45*60);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 8*3600 + 15*60, 0, DateTimeUtils::set(0, (9*3600+45*60)), false);

    BOOST_REQUIRE_EQUAL(res1.size(), 2);

    res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 5);
    BOOST_REQUIRE_EQUAL(res.items[0].stop_points.size(), 2);
    BOOST_REQUIRE_EQUAL(res.items[1].stop_points.size(), 1);
    BOOST_REQUIRE_EQUAL(res.items[2].stop_points.size(), 3);
    BOOST_REQUIRE_EQUAL(res.items[3].stop_points.size(), 1);
    BOOST_REQUIRE_EQUAL(res.items[4].stop_points.size(), 2);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, d.stop_points_map["stop1"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[2]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(res.items[3].stop_points[0]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(res.items[4].stop_points[0]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(res.items[4].stop_points[1]->idx, d.stop_points_map["stop4"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].departure.time_of_day().total_seconds(), 8*3600+20*60);
    BOOST_CHECK_EQUAL(res.items[4].arrival.time_of_day().total_seconds(), 8*3600+45*60);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 8*3600 + 15*60, 0, DateTimeUtils::set(0, (8*3600+45*60)), false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res = res1.back();

    BOOST_REQUIRE_EQUAL(res.items.size(), 5);
    BOOST_REQUIRE_EQUAL(res.items[0].stop_points.size(), 2);
    BOOST_REQUIRE_EQUAL(res.items[1].stop_points.size(), 1);
    BOOST_REQUIRE_EQUAL(res.items[2].stop_points.size(), 3);
    BOOST_REQUIRE_EQUAL(res.items[3].stop_points.size(), 1);
    BOOST_REQUIRE_EQUAL(res.items[4].stop_points.size(), 2);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[0]->idx, d.stop_points_map["stop1"]->idx);
    BOOST_CHECK_EQUAL(res.items[0].stop_points[1]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[1].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[0]->idx, d.stop_points_map["stop2"]->idx);
    BOOST_CHECK_EQUAL(res.items[2].stop_points[2]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(res.items[3].stop_points[0]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(res.items[4].stop_points[0]->idx, d.stop_points_map["stop3"]->idx);
    BOOST_CHECK_EQUAL(res.items[4].stop_points[1]->idx, d.stop_points_map["stop4"]->idx);

    BOOST_CHECK_EQUAL(res.items[0].departure.time_of_day().total_seconds(), 8*3600+20*60);
    BOOST_CHECK_EQUAL(res.items[4].arrival.time_of_day().total_seconds(), 8*3600+45*60);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 8*3600 + 15*60, 0, DateTimeUtils::set(0, (8*3600+45*60)-1), false);

    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}


BOOST_AUTO_TEST_CASE(pam_veille) {
    ed::builder b("20120614");
    b.vj("A", "11111111", "", true)("stop1", 3*60)("stop2", 20*60);
    b.vj("B", "01", "", true)("stop0", 23*3600)("stop2", 24*3600 + 30*60)("stop3", 24*3600 + 40*60);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop2"], d.stop_areas_map["stop3"], 5*60, 0, DateTimeUtils::inf, false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);
}


BOOST_AUTO_TEST_CASE(pam_3) {
    ed::builder b("20120614");
    b.vj("A")("stop1", 23*3600)("stop2", 24*3600 + 5 * 60);
    b.vj("B")("stop1", 23*3600)("stop3", 23*3600 + 5 * 60);
    b.vj("C1")("stop2", 10*60)("stop3", 20*60)("stop4", 30*60);
    b.vj("C1")("stop2", 23*3600)("stop3", 23*3600 + 10*60)("stop4", 23*3600 + 40*60);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 5*60, 0, DateTimeUtils::inf, false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    BOOST_CHECK_EQUAL(res1[0].items[2].arrival.time_of_day().total_seconds(), 23*3600 + 40 * 60);
}

BOOST_AUTO_TEST_CASE(sn_debut) {
    ed::builder b("20120614");
    b.vj("A","11111111", "", true)("stop1", 8*3600)("stop2", 8*3600 + 20*60);
    b.vj("B","11111111", "", true)("stop1", 9*3600)("stop2", 9*3600 + 20*60);

    std::vector<std::pair<navitia::type::idx_t, bt::time_duration>> departs, destinations;
    departs.push_back(std::make_pair(0, bt::seconds(10 * 60)));
    destinations.push_back(std::make_pair(1,bt::seconds(0)));

    std::vector<std::pair<type::EntryPoint, std::vector<std::pair<navitia::type::idx_t, bt::time_duration>> > > multi_departures, multi_arrivals;
    multi_departures.push_back(std::make_pair(type::EntryPoint(), departs));
    multi_arrivals.push_back(std::make_pair(type::EntryPoint(), destinations));

    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);

    auto res1 = raptor.compute_all(multi_departures, multi_arrivals, DateTimeUtils::set(0, 8*3600), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    BOOST_CHECK_EQUAL(res1.back().items[0].arrival.time_of_day().total_seconds(), 9*3600 + 20 * 60);
}


BOOST_AUTO_TEST_CASE(prolongement_service) {
    ed::builder b("20120614");
    b.vj("A", "1111111", "", true)("stop1", 8*3600)("stop2", 8*3600+10*60);
    b.vj("B", "1111111", "", true)("stop4", 8*3600+15*60)("stop3", 8*3600 + 20*60);
    b.journey_pattern_point_connection("A", "B");
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 5*60, 0, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    BOOST_CHECK_EQUAL(res1.back().items[2].arrival.time_of_day().total_seconds(), 8*3600 + 20*60);
}


BOOST_AUTO_TEST_CASE(itl) {
    ed::builder b("20120614");
    b.vj("A")("stop1",8*3600+10*60, 8*3600 + 10*60,1)("stop2",8*3600+15*60,8*3600+15*60,1)("stop3", 8*3600+20*60);
    b.vj("B")("stop1",9*3600)("stop2",10*3600);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 5*60, 0, DateTimeUtils::inf, false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    BOOST_CHECK_EQUAL(res1.back().items[0].arrival.time_of_day().total_seconds(), 10*3600);


    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 5*60, 0, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    BOOST_CHECK_EQUAL(res1.back().items[0].arrival.time_of_day().total_seconds(), 8*3600+20*60);
}


BOOST_AUTO_TEST_CASE(mdi) {
    ed::builder b("20120614");

    b.vj("B")("stop1",17*3600, 17*3600,std::numeric_limits<uint32_t>::max(), true, false)("stop2", 17*3600+15*60)("stop3",17*3600+30*60, 17*3600+30*60,std::numeric_limits<uint32_t>::max(), true, false);
    b.vj("C")("stop4",16*3600, 16*3600,std::numeric_limits<uint32_t>::max(), true, true)("stop5", 16*3600+15*60)("stop6",16*3600+30*60, 16*3600+30*60,std::numeric_limits<uint32_t>::max(), false, true);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop3"], 5*60, 0, DateTimeUtils::inf, false);
    BOOST_CHECK_EQUAL(res1.size(), 0);
    res1 = raptor.compute(d.stop_areas_map["stop2"], d.stop_areas_map["stop3"], 5*60, 0, DateTimeUtils::inf, false);
    BOOST_CHECK_EQUAL(res1.size(), 1);

    res1 = raptor.compute(d.stop_areas_map["stop4"], d.stop_areas_map["stop6"], 5*60, 0, DateTimeUtils::inf, false);
    BOOST_CHECK_EQUAL(res1.size(), 0);
    res1 = raptor.compute(d.stop_areas_map["stop4"],d.stop_areas_map["stop5"], 5*60, 0, DateTimeUtils::inf, false);
    BOOST_CHECK_EQUAL(res1.size(), 1);
}


BOOST_AUTO_TEST_CASE(multiples_vj) {
    ed::builder b("20120614");
    b.vj("A1")("stop1", 8*3600)("stop2", 8*3600+10*60);
    b.vj("A2")("stop1", 8*3600 + 15*60 )("stop2", 8*3600+20*60);
    b.vj("B1")("stop2", 8*3600 + 25*60)("stop3", 8*3600+30*60);
    b.vj("B2")("stop2", 8*3600 + 35*60)("stop3", 8*3600+40*60);
    b.vj("C")("stop3", 8*3600 + 45*60)("stop4", 8*3600+50*60);
    b.vj("E1")("stop1", 8*3600)("stop5", 8*3600 + 5*60);
    b.vj("E2")("stop5", 8*3600 + 10 * 60)("stop1", 8*3600 + 12*60);

    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;


    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop4"], 5*60, 0, DateTimeUtils::inf, false);

    BOOST_CHECK_EQUAL(res1.size(), 1);
}


BOOST_AUTO_TEST_CASE(freq_vj) {
    ed::builder b("20120614");
    b.vj("A1")("stop1", 8*3600)("stop2", 8*3600+10*60).frequency(8*3600,18*3600,5*60);

    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;


    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 8*3600, 0, DateTimeUtils::inf, false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    BOOST_CHECK_EQUAL(res1[0].items[0].arrival.time_of_day().total_seconds(), 8*3600 + 10*60);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 9*3600, 0, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    BOOST_CHECK_EQUAL(res1[0].items[0].arrival.time_of_day().total_seconds(), 9*3600 + 10*60);
}


BOOST_AUTO_TEST_CASE(freq_vj_pam) {
    ed::builder b("20120614");
    b.vj("A1")("stop1", 8*3600)("stop2", 8*3600+10*60).frequency(8*3600,26*3600,5*60);

    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 23*3600, 0, DateTimeUtils::inf, false);



    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    BOOST_CHECK_EQUAL(res1[0].items[0].arrival.time_of_day().total_seconds(), 23*3600 + 10*60);

    /*auto*/ res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 24*3600, 0, DateTimeUtils::inf, false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res1[0].items[0].arrival, b.data)), 1);
    BOOST_CHECK_EQUAL(res1[0].items[0].arrival.time_of_day().total_seconds(), (24*3600 + 10*60)% DateTimeUtils::SECONDS_PER_DAY);

    /*auto*/ res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 25*3600, 0, DateTimeUtils::inf, false);

    BOOST_REQUIRE_EQUAL(res1.size(), 1);
    BOOST_CHECK_EQUAL(DateTimeUtils::date(to_datetime(res1[0].items[0].arrival, b.data)), 1);
    BOOST_CHECK_EQUAL(res1[0].items[0].arrival.time_of_day().total_seconds(), (25*3600 + 10*60)% DateTimeUtils::SECONDS_PER_DAY);
}


BOOST_AUTO_TEST_CASE(max_duration){
    ed::builder b("20120614");
    b.vj("A")("stop1", 8000, 8050)("stop2", 8100,8150);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7900, 0, DateTimeUtils::inf, false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7900, 0, DateTimeUtils::set(0, 8101), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 1);

    res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7900, 0, DateTimeUtils::set(0, 8099), false);
    BOOST_REQUIRE_EQUAL(res1.size(), 0);
}


BOOST_AUTO_TEST_CASE(max_transfers){
    ed::builder b("20120614");
    b.vj("A")("stop1", 8000, 8050)("stop2", 81000,81500);
    b.vj("B")("stop1",8000)("stop3",8500);
    b.vj("C")("stop3",9000)("stop2",11000);
    b.vj("D")("stop3",9000)("stop4",9500);
    b.vj("E")("stop4",10000)("stop2",10500);
    b.data.pt_data.index();
    b.data.build_raptor();
    b.data.build_uri();
    RAPTOR raptor(b.data);
    type::PT_Data & d = b.data.pt_data;

    for(uint32_t nb_transfers=0; nb_transfers<=2; ++nb_transfers) {
//        type::Properties p;
        auto res1 = raptor.compute(d.stop_areas_map["stop1"], d.stop_areas_map["stop2"], 7900, 0, DateTimeUtils::inf, false, true, type::AccessibiliteParams(), nb_transfers);
        BOOST_REQUIRE(res1.size()>=1);
        for(auto r : res1) {
            BOOST_REQUIRE(r.nb_changes <= nb_transfers);
        }
    }
}

