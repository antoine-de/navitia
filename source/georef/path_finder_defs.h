#include "street_network.h"
#include "type/data.h"
#include "georef.h"
#include <chrono>

namespace navitia { namespace georef {

inline bt::time_duration get_walking_duration(const double val) {
    return bt::seconds(val / default_speed[type::Mode_e::Walking]);
}

template <typename graph_wrapper_trait>
PathFinder<graph_wrapper_trait>::PathFinder(const GeoRef& gref) : geo_ref(gref) {}

template <typename graph_wrapper_trait>
void
PathFinder<graph_wrapper_trait>::init(const type::GeographicalCoord& start_coord, nt::Mode_e mode, const float speed_factor) {
    computation_launch = false;
    // we look for the nearest edge from the start coorinate in the right transport mode (walk, bike, car, ...) (ie offset)
    this->mode = mode;
    this->speed_factor = speed_factor; //the speed factor is the factor we have to multiply the edge cost with
    nt::idx_t offset = this->geo_ref.offsets[mode];
    this->start_coord = start_coord;
    starting_edge = ProjectionData(start_coord, this->geo_ref, offset, this->geo_ref.pl);

    //we initialize the distances to the maximum value
    size_t n = boost::num_vertices(geo_ref.graph);
    distances.assign(n, bt::pos_infin);
    //for the predecessors no need to clean the values, the important one will be updated during search
    predecessors.resize(n);

    if (starting_edge.found) {
        //durations initializations
        distances[starting_edge.source] = get_walking_duration(starting_edge.source_distance); //for the projection, we use the default walking speed.
        distances[starting_edge.target] = get_walking_duration(starting_edge.target_distance);
        predecessors[starting_edge.source] = starting_edge.source;
        predecessors[starting_edge.target] = starting_edge.target;
    }
}

template <typename graph_wrapper_trait>
std::vector<std::pair<type::idx_t, bt::time_duration>>
PathFinder<graph_wrapper_trait>::find_nearest_stop_points(bt::time_duration radius, const proximitylist::ProximityList<type::idx_t>& pl) {
if (! starting_edge.found)
        return {};

    // On trouve tous les élements à moins radius mètres en vol d'oiseau
    float crow_flies_dist = radius.total_seconds() * speed_factor * georef::default_speed[mode];
    std::vector< std::pair<nt::idx_t, type::GeographicalCoord> > elements = pl.find_within(start_coord, crow_flies_dist);
    if(elements.empty())
        return {};

    computation_launch = true;
    std::vector<std::pair<type::idx_t, bt::time_duration>> result;

    // On lance un dijkstra depuis les deux nœuds de départ
    try {
        dijkstra(starting_edge.source, distance_visitor(radius, distances));
    } catch(DestinationFound){}

    try {
        dijkstra(starting_edge.target, distance_visitor(radius, distances));
    } catch(DestinationFound){}

    const auto max = bt::pos_infin;

    for (auto element: elements) {
        ProjectionData projection = this->geo_ref.projected_stop_points[element.first][mode];
        // Est-ce que le stop point a pu être raccroché au street network
        if(projection.found){
            bt::time_duration best_dist = max;
            if (distances[projection.source] < max) {
                best_dist = distances[projection.source] + get_walking_duration(projection.source_distance);
            }
            if (distances[projection.target] < max) {
                best_dist = std::min(best_dist, distances[projection.target] + get_walking_duration(projection.target_distance));
            }
            if (best_dist < radius) {
                result.push_back(std::make_pair(element.first, best_dist));
            }
        }
    }
    return result;
}

template <typename graph_wrapper_trait>
bt::time_duration
PathFinder<graph_wrapper_trait>::get_distance(type::idx_t target_idx) {
    constexpr auto max = bt::pos_infin;

    if (! starting_edge.found)
        return max;
    assert(boost::edge(starting_edge.source, starting_edge.target, geo_ref.graph).second);

    ProjectionData target = this->geo_ref.projected_stop_points[target_idx][mode];

    auto nearest_edge = update_path(target);

    return nearest_edge.first;
}

template <typename graph_wrapper_trait>
std::pair<bt::time_duration, vertex_t>
PathFinder<graph_wrapper_trait>::find_nearest_vertex(const ProjectionData& target) const {
    constexpr auto max = bt::pos_infin;
    if (! target.found)
        return {max, {}};

    if (distances[target.source] == max) //if one distance has not been reached, both have not been reached
        return {max, {}};

    auto source_dist = distances[target.source] + get_walking_duration(target.source_distance);
    auto target_dist = distances[target.target] + get_walking_duration(target.target_distance);

    if (target_dist < source_dist)
        return {target_dist, target.target};

    return {source_dist, target.source};
}

template <typename graph_wrapper_trait>
Path
PathFinder<graph_wrapper_trait>::get_path(type::idx_t idx) {
    if (! computation_launch)
        return {};
    ProjectionData projection = this->geo_ref.projected_stop_points[idx][mode];

    auto nearest_edge = find_nearest_vertex(projection);

    return get_path(projection, nearest_edge);
}

template <typename graph_wrapper_trait>
Path
PathFinder<graph_wrapper_trait>::get_path(const ProjectionData& target, std::pair<bt::time_duration, vertex_t> nearest_edge) {
    if (! computation_launch || ! target.found || nearest_edge.first == bt::pos_infin)
        return {};

    auto result = this->geo_ref.build_path(nearest_edge.second, this->predecessors);
    add_projections_to_path(result, true);

    result.duration = nearest_edge.first;

    //we need to put the end projections too
    edge_t end_e = boost::edge(target.source, target.target, geo_ref.graph).first;
    Edge end_edge = geo_ref.graph[end_e];
    nt::idx_t last_way_idx = result.path_items.back().way_idx;
    if (end_edge.way_idx != last_way_idx) {
        PathItem item;
        item.way_idx = end_edge.way_idx;
        result.path_items.push_back(item);
    }
    auto& back_coord_list = result.path_items.back().coordinates;
    if (back_coord_list.empty() || back_coord_list.back() != target.projected)
        back_coord_list.push_back(target.projected);

    return result;
}

template <typename graph_wrapper_trait>
Path
PathFinder<graph_wrapper_trait>::compute_path(const type::GeographicalCoord& target_coord) {
    ProjectionData dest(target_coord, geo_ref, geo_ref.pl);

    auto best_pair = update_path(dest);

    return get_path(dest, best_pair);
}

template <typename graph_wrapper_trait>
void
PathFinder<graph_wrapper_trait>::add_projections_to_path(Path& p, bool append_to_begin) const {
    auto item_to_update = [append_to_begin](Path& p) -> PathItem& { return (append_to_begin ? p.path_items.front() : p.path_items.back()); };
    auto add_in_path = [append_to_begin](Path& p, const PathItem& item) {
        return (append_to_begin ? p.path_items.push_front(item) : p.path_items.push_back(item));
    };

    edge_t start_e = boost::edge(starting_edge.source, starting_edge.target, geo_ref.graph).first;
    Edge start_edge = geo_ref.graph[start_e];

    //we aither add the starting coordinate to the first path item or create a new path item if it was another way
    nt::idx_t first_way_idx = (p.path_items.empty() ? type::invalid_idx : item_to_update(p).way_idx);
    if (start_edge.way_idx != first_way_idx || first_way_idx == type::invalid_idx) {
        if (! p.path_items.empty() && item_to_update(p).way_idx == type::invalid_idx) { //there can be an item with no way, so we will update this item
            item_to_update(p).way_idx = start_edge.way_idx;
        }
        else {
            PathItem item;
            item.way_idx = start_edge.way_idx;
            if (!p.path_items.empty()) {
                //still complexifying stuff... TODO: simplify this
                //we want the projection to be done with the previous transportation mode
                switch (item_to_update(p).transportation) {
                case georef::PathItem::TransportCaracteristic::Walk:
                case georef::PathItem::TransportCaracteristic::Car:
                case georef::PathItem::TransportCaracteristic::Bike:
                    item.transportation = item_to_update(p).transportation;
                    break;
                    //if we were switching between walking and biking, we need to take either
                    //the previous or the next transportation mode depending on 'append_to_begin'
                case georef::PathItem::TransportCaracteristic::BssTake:
                    item.transportation = (append_to_begin ? georef::PathItem::TransportCaracteristic::Walk
                                                           : georef::PathItem::TransportCaracteristic::Bike);
                    break;
                case georef::PathItem::TransportCaracteristic::BssPutBack:
                    item.transportation = (append_to_begin ? georef::PathItem::TransportCaracteristic::Bike
                                                           : georef::PathItem::TransportCaracteristic::Walk);
                    break;
                default:
                    throw navitia::exception("unhandled transportation carac case");
                }
            }
            add_in_path(p, item);
        }
    }
    auto& coord_list = item_to_update(p).coordinates;
    if (append_to_begin) {
        if (coord_list.empty() || coord_list.front() != starting_edge.projected)
            coord_list.push_front(starting_edge.projected);
    }
    else {
        if (coord_list.empty() || coord_list.back() != starting_edge.projected)
            coord_list.push_back(starting_edge.projected);
    }
}

template <typename graph_wrapper_trait>
std::pair<bt::time_duration, vertex_t>
PathFinder<graph_wrapper_trait>::update_path(const ProjectionData& target) {
    constexpr auto max = bt::pos_infin;
    if (! target.found)
        return {max, {}};
    assert(boost::edge(target.source, target.target, geo_ref.graph).second );

    computation_launch = true;

    if (distances[target.source] == max || distances[target.target] == max) {
        bool found = false;
        try {
            dijkstra(starting_edge.source, target_all_visitor({target.source, target.target}));
        } catch(DestinationFound) { found = true; }

        //if no way has been found, we can stop the search
        if ( ! found ) {
            LOG4CPLUS_WARN(log4cplus::Logger::getInstance("Logger"), "unable to find a way from start edge ["
                           << starting_edge.source << "-" << starting_edge.target
                           << "] to [" << target.source << "-" << target.target << "]");
            return {max, {}};
        }
        try {
            dijkstra(starting_edge.target, target_all_visitor({target.source, target.target}));
        } catch(DestinationFound) { found = true; }

    }
    //if we succeded in the first search, we must have found one of the other distances
    assert(distances[target.source] != max && distances[target.target] != max);

    return find_nearest_vertex(target);
}

}}
