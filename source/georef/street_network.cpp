#include "street_network.h"
#include "type/data.h"
#include "georef.h"
#include <chrono>
#include "path_finder_defs.h"

namespace navitia { namespace georef {

StreetNetwork::StreetNetwork(const GeoRef &geo_ref) :
    geo_ref(geo_ref),
    departure_path_finder(geo_ref),
    arrival_path_finder(geo_ref)
{}

void StreetNetwork::init(const type::EntryPoint& start, boost::optional<const type::EntryPoint&> end) {
    departure_path_finder.init(start.coordinates, start.streetnetwork_params.mode, start.streetnetwork_params.speed_factor);

    if (end) {
        arrival_path_finder.init((*end).coordinates, (*end).streetnetwork_params.mode, (*end).streetnetwork_params.speed_factor);
    }
}

bool StreetNetwork::departure_launched() const {return departure_path_finder.computation_launch;}
bool StreetNetwork::arrival_launched() const {return arrival_path_finder.computation_launch;}

std::vector<std::pair<type::idx_t, bt::time_duration>>
StreetNetwork::find_nearest_stop_points(bt::time_duration radius, const proximitylist::ProximityList<type::idx_t>& pl, bool use_second) {
    // delegate to the arrival or departure pathfinder
    // results are store to build the routing path after the transportation routing computation
    if (use_second)
        return arrival_path_finder.find_nearest_stop_points(radius, pl);
    return departure_path_finder.find_nearest_stop_points(radius, pl);
}

bt::time_duration StreetNetwork::get_distance(type::idx_t target_idx, bool use_second) {
    if (use_second)
       return arrival_path_finder.get_distance(target_idx);
    return departure_path_finder.get_distance(target_idx);
}

Path StreetNetwork::get_path(type::idx_t idx, bool use_second) {
    Path result;
    if (! use_second) {
        result = departure_path_finder.get_path(idx);

        if (! result.path_items.empty())
            result.path_items.front().coordinates.push_front(departure_path_finder.starting_edge.projected);
    } else {
        result = arrival_path_finder.get_path(idx);

        //we have to reverse the path
        std::reverse(result.path_items.begin(), result.path_items.end());
        for (auto& item : result.path_items) {
            std::reverse(item.coordinates.begin(), item.coordinates.end());
            //we have to reverse the directions too
            item.angle *= -1;
        }

        if (! result.path_items.empty()) {
            //no direction for the last elt
            result.path_items.back().angle = 0;
            result.path_items.back().coordinates.push_back(arrival_path_finder.starting_edge.projected);
        }
    }

    return result;
}

Path StreetNetwork::get_direct_path() {
    if(!departure_launched() || !arrival_launched())
        return {};
    //Cherche s'il y a des nœuds en commun, et retient le chemin le plus court
    size_t num_vertices = boost::num_vertices(geo_ref.graph);

    bt::time_duration min_dist = bt::pos_infin;
    vertex_t target = std::numeric_limits<size_t>::max();
    for(vertex_t u = 0; u != num_vertices; ++u) {
        if((departure_path_finder.distances[u] != bt::pos_infin)
                && (arrival_path_finder.distances[u] != bt::pos_infin)
                && ((departure_path_finder.distances[u] + arrival_path_finder.distances[u]) < min_dist)) {
            target = u;
            min_dist = departure_path_finder.distances[u] + arrival_path_finder.distances[u];
        }
    }

    //Construit l'itinéraire
    if(min_dist == bt::pos_infin)
        return {};

    Path result = this->geo_ref.combine_path(target, departure_path_finder.predecessors, arrival_path_finder.predecessors);
    departure_path_finder.add_projections_to_path(result, true);
    arrival_path_finder.add_projections_to_path(result, false);

    result.path_items.back().angle = 0;

    return result;
}

}}
