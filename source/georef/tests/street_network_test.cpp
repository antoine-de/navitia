#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE test_street_network
#include <boost/test/unit_test.hpp>
#include "georef/georef.h"
#include "builder.h"
#include "type/data.h"

//to be able to test private member.
#define private public
#include"georef/street_network.h"
#undef private
#include "georef/path_finder_defs.h"
#include "georef/tests/time_utils.h"

using namespace navitia::georef;

using namespace navitia;
namespace bt = boost::posix_time;

struct computation_results {
    bt::time_duration duration; //asked duration
    std::vector<bt::time_duration> durations_matrix; //duration matrix
    std::vector<vertex_t> predecessor;

    computation_results(bt::time_duration d, const PathFinder<identity_graph_wrapper>& worker) : duration(d), durations_matrix(worker.distances), predecessor(worker.predecessors) {}

    bool operator ==(const computation_results& other) {

        BOOST_CHECK_EQUAL(other.duration, duration);

        BOOST_REQUIRE_EQUAL(other.durations_matrix.size(), durations_matrix.size());

        for (size_t i = 0 ; i < durations_matrix.size() ; ++i) {
            BOOST_CHECK_EQUAL(other.durations_matrix.at(i), durations_matrix.at(i));
        }

        BOOST_CHECK(predecessor == other.predecessor);

        return true;
    }
};

std::string get_name(int i, int j) { std::stringstream ss; ss << i << "_" << j; return ss.str(); }
bool almost_equal(float a, float b) {
    return fabs(a - b) < 0.00001;
}

/**
  * The aim of the test is to check that the street network answer give the same answer
  * to multiple get_distance question
  *
  **/
BOOST_AUTO_TEST_CASE(idempotence) {
    //graph creation
    type::Data data;
    GeoRef geo_ref;
    GraphBuilder b(geo_ref);
    size_t square_size(10);

    //we build a dumb square graph
    for (size_t i = 0; i < square_size ; ++i) {
        for (size_t j = 0; j < square_size ; ++j) {
            std::string name(get_name(i, j));
            b(name, i, j);
        }
    }
    for (size_t i = 0; i < square_size - 1; ++i) {
        for (size_t j = 0; j < square_size - 1; ++j) {
            std::string name(get_name(i, j));

            //we add edge to the next vertex (the value is not important)
            b.add_edge(name, get_name(i, j + 1), bt::seconds((i + j) * j));
            b.add_edge(name, get_name(i + 1, j), bt::seconds((i + j) * i));
        }
    }

    PathFinder<identity_graph_wrapper> worker(geo_ref);

    //we project 2 stations
    type::GeographicalCoord start;
    start.set_xy(2., 2.);

    type::StopPoint* sp = new type::StopPoint();
    sp->coord.set_xy(8., 8.);
    sp->idx = 0;
    data.pt_data.stop_points.push_back(sp);
    geo_ref.init();
    geo_ref.project_stop_points(data.pt_data.stop_points);

    const GeoRef::ProjectionByMode& projections = geo_ref.projected_stop_points[sp->idx];
    const ProjectionData proj = projections[type::Mode_e::Walking];

    BOOST_REQUIRE(proj.found); //we have to be able to project this point (on the walking graph)

    geo_ref.build_proximity_list();

    type::idx_t target_idx(sp->idx);

    worker.init(start, type::Mode_e::Walking, georef::default_speed[type::Mode_e::Walking]);

    auto distance = worker.get_distance(target_idx);

    //we have to find a way to get there
    BOOST_REQUIRE_NE(distance, bt::pos_infin);

    std::cout << "distance " << distance
              << " proj distance to source " << proj.source_distance
              << " proj distance to target " << proj.target_distance
              << " distance to source " << worker.distances[proj.source]
              << " distance to target " << worker.distances[proj.target] << std::endl;

    // the distance matrix also has to be updated
    BOOST_CHECK(worker.distances[proj.source] + bt::seconds(proj.source_distance / default_speed[type::Mode_e::Walking]) == distance//we have to take into account the projection distance
                    || worker.distances[proj.target] + bt::seconds(proj.target_distance / default_speed[type::Mode_e::Walking]) == distance);

    computation_results first_res {distance, worker};

    //we ask again with the init again
    {
        worker.init(start, type::Mode_e::Walking, georef::default_speed[type::Mode_e::Walking]);
        auto other_distance = worker.get_distance(target_idx);

        computation_results other_res {other_distance, worker};

        //we have to find a way to get there
        BOOST_REQUIRE_NE(other_distance, bt::pos_infin);
        // the distance matrix  also has to be updated
        BOOST_CHECK(worker.distances[proj.source] + bt::seconds(proj.source_distance / default_speed[type::Mode_e::Walking]) == other_distance
                        || worker.distances[proj.target] + bt::seconds(proj.target_distance / default_speed[type::Mode_e::Walking]) == other_distance);

        BOOST_REQUIRE(first_res == other_res);
    }

    //we ask again without a init
    {
        auto other_distance = worker.get_distance(target_idx);

        computation_results other_res {other_distance, worker};
        //we have to find a way to get there
        BOOST_CHECK_NE(other_distance, bt::pos_infin);

        BOOST_CHECK(worker.distances[proj.source] + bt::seconds(proj.source_distance / default_speed[type::Mode_e::Walking]) == other_distance
                        || worker.distances[proj.target] + bt::seconds(proj.target_distance / default_speed[type::Mode_e::Walking]) == other_distance);

        BOOST_CHECK(first_res == other_res);
    }
}

BOOST_AUTO_TEST_CASE(reverse_graph){
    using namespace navitia::type;

    GeoRef sn;
    GraphBuilder b(sn);

    /*
     *    j       i      h     g       f
     *    o---<---o--<---o--<--o--<----o
     *    |                            |
     *    v                            ^
     *    |                            |
     *    o--->---o-->---o-->--o-->----o
     *    a       b      c     d       e
     */

    b("a",0,0)("b",100,0)("c",200,0)("d",300,0)("e",400,0);
    b("j",0,100)("i",100,100)("h",200,100)("g",300,100)("f",400,100);

    b("a","b",100_s)("b","c",100_s)("c","d",100_s)("d","e",100_s);
    b("e","f",100_s)("f","g",100_s)("g","h",100_s)("h","i",100_s)("i","j",100_s)("j","a",100_s);
    size_t cpt(0);
    sn.graph[b.get("a","b")].way_idx = cpt++;
    sn.graph[b.get("b","c")].way_idx = cpt++;
    sn.graph[b.get("c","d")].way_idx = cpt++;
    sn.graph[b.get("d","e")].way_idx = cpt++;
    sn.graph[b.get("e","f")].way_idx = cpt++;
    sn.graph[b.get("f","g")].way_idx = cpt++;
    sn.graph[b.get("g","h")].way_idx = cpt++;
    sn.graph[b.get("h","i")].way_idx = cpt++;
    sn.graph[b.get("i","j")].way_idx = cpt++;
    sn.graph[b.get("j","a")].way_idx = cpt++;

    sn.init();
    PathFinder<identity_graph_wrapper> path_finder(sn);

    path_finder.init({0, 0, true}, Mode_e::Walking, 1); //starting from a
    Path p = path_finder.compute_path({400, 0, true}); //going to e

    for ( auto item :p.path_items) {
        std::cout << "item : " << item.way_idx << std::endl;
        for ( auto coord: item.coordinates) {
            std::cout << "-" << coord.lon() / GeographicalCoord::M_TO_DEG
                      << "," << coord.lat() / GeographicalCoord::M_TO_DEG << std::endl;
        }
    }

    //the normal path should be a > b > c > d > e
    BOOST_REQUIRE_EQUAL(p.path_items.size(), 5); //5 since the projection is done on e-f
    BOOST_CHECK_EQUAL(p.path_items[0].way_idx, 0);
    BOOST_CHECK_EQUAL(p.path_items[1].way_idx, 1);
    BOOST_CHECK_EQUAL(p.path_items[2].way_idx, 2);
    BOOST_CHECK_EQUAL(p.path_items[3].way_idx, 3);
    BOOST_CHECK_EQUAL(p.path_items[4].way_idx, 4);
    BOOST_CHECK_EQUAL(p.path_items[4].duration, 0_s); //empty item for the last one, just for the projection

    //now we try the reverse way, the path should be a > j > i > h > g > f > e

    PathFinder<reverse_graph_wrapper> reverse_path_finder(sn);
    reverse_path_finder.init({0, 0, true}, Mode_e::Walking, 1); //starting from a
    try {
    p = reverse_path_finder.compute_path({400, 0, true}); //going still to e
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    BOOST_REQUIRE_EQUAL(p.path_items.size(), 7);
    BOOST_CHECK_EQUAL(p.path_items[0].way_idx, 0);
    BOOST_CHECK_EQUAL(p.path_items[0].duration, 0_s); //empty item for the first one, just for the projection
    BOOST_CHECK_EQUAL(p.path_items[1].way_idx, 9);
    BOOST_CHECK_EQUAL(p.path_items[2].way_idx, 8);
    BOOST_CHECK_EQUAL(p.path_items[3].way_idx, 7);
    BOOST_CHECK_EQUAL(p.path_items[4].way_idx, 6);
    BOOST_CHECK_EQUAL(p.path_items[5].way_idx, 5);
    BOOST_CHECK_EQUAL(p.path_items[6].way_idx, 4);
}

// test that if all edges are bidirect, the reverse path finder and the normal one find the same results
BOOST_AUTO_TEST_CASE(reverse_graph_on_bidirect_ways) {
    using namespace navitia::type;
    //StreetNetwork sn;
    GeoRef sn;
    GraphBuilder b(sn);
    Way w;
    w.name = "Jaures"; sn.add_way(w);
    w.name = "Hugo"; sn.add_way(w);

    b("a", 0, 0)("b", 1, 1)("c", 2, 2)("d", 3, 3)("e", 4, 4);
    b("a", "b")("b", "a")("b","c")("c","b")("c","d")("d","c")("d","e")("e","d");
    sn.graph[b.get("a","b")].way_idx = 0;
    sn.graph[b.get("b","a")].way_idx = 0;
    sn.graph[b.get("b","c")].way_idx = 0;
    sn.graph[b.get("c","b")].way_idx = 0;
    sn.graph[b.get("c","d")].way_idx = 1;
    sn.graph[b.get("d","c")].way_idx = 1;
    sn.graph[b.get("d","e")].way_idx = 1;
    sn.graph[b.get("e","d")].way_idx = 1;

    sn.init();

    PathFinder<identity_graph_wrapper> path_finder(sn);
    path_finder.init({0, 0, true}, Mode_e::Walking, 1); //starting from a
    Path p = path_finder.compute_path({4, 4, true}); //going to e
    BOOST_REQUIRE_EQUAL(p.path_items.size(), 2);
    BOOST_CHECK_EQUAL(p.path_items[0].way_idx, 0);
    BOOST_CHECK_EQUAL(p.path_items[1].way_idx, 1);

    PathFinder<reverse_graph_wrapper> reverse_path_finder(sn);
    reverse_path_finder.init({0, 0, true}, Mode_e::Walking, 1); //starting from a
    Path reverse_path = reverse_path_finder.compute_path({4, 4, true}); //going to e
    BOOST_REQUIRE_EQUAL(reverse_path.path_items.size(), p.path_items.size());
    BOOST_CHECK_EQUAL(reverse_path.path_items[0].way_idx,  p.path_items[0].way_idx);
    BOOST_CHECK(reverse_path.path_items[0].coordinates == p.path_items[0].coordinates);
    BOOST_CHECK_EQUAL(reverse_path.path_items[1].way_idx,  p.path_items[1].way_idx);
    BOOST_CHECK(reverse_path.path_items[1].coordinates == p.path_items[1].coordinates);
}
