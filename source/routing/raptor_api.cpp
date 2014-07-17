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

#include "raptor_api.h"
#include "type/pb_converter.h"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "type/datetime.h"
#include <unordered_set>
#include <chrono>
#include "type/meta_data.h"
#include "fare/fare.h"


namespace navitia { namespace routing {

void fill_section(pbnavitia::Section *pb_section, const type::VehicleJourney* vj,
        const nt::Data & d, bt::ptime now, bt::time_period action_period) {

    if (vj->has_boarding()){
        pb_section->set_type(pbnavitia::boarding);
        return;
    }
    if (vj->has_landing()){
        pb_section->set_type(pbnavitia::landing);
        return;
    }
    auto* vj_pt_display_information = pb_section->mutable_pt_display_informations();
    auto* add_info_vehicle_journey = pb_section->mutable_add_info_vehicle_journey();
    fill_pb_object(vj, d, vj_pt_display_information, 0, now, action_period);
    fill_pb_object(vj, d, add_info_vehicle_journey, 0, now, action_period);
}



pbnavitia::Response make_pathes(const std::vector<navitia::routing::Path>& paths,
        const nt::Data& d, georef::StreetNetwork& worker,
        const type::EntryPoint& origin, const type::EntryPoint& destination,
        const std::vector<bt::ptime>& datetimes, const bool clockwise, const bool show_codes) {
    EnhancedResponse enhanced_response; //wrapper around raw protobuff response to handle ids
    pbnavitia::Response& pb_response = enhanced_response.response;

    bt::ptime now = bt::second_clock::local_time();

    auto temp = worker.get_direct_path();
    if(!temp.path_items.empty()) {
        pb_response.set_response_type(pbnavitia::ITINERARY_FOUND);

        //for each date time we add a direct street journey
        for(bt::ptime datetime : datetimes) {
            pbnavitia::Journey* pb_journey = pb_response.add_journeys();
            pb_journey->set_requested_date_time(navitia::to_posix_timestamp(datetime));
            pb_journey->set_duration(temp.duration.total_seconds());

            bt::ptime departure;
            if (clockwise) {
                departure = datetime;
            } else {
                departure = datetime - temp.duration.to_posix();
            }
            fill_street_sections(enhanced_response, origin, temp, d, pb_journey, departure);

            const auto int_departure = navitia::to_posix_timestamp(departure);
            const auto int_arrival = navitia::to_posix_timestamp(departure + temp.duration.to_posix());
            pb_journey->set_departure_date_time(int_departure);
            pb_journey->set_arrival_date_time(int_arrival);
        }
    }

    pb_response.set_response_type(pbnavitia::ITINERARY_FOUND);

    for(Path path : paths) {
        bt::ptime departure_time = bt::pos_infin,
                          arrival_time = bt::pos_infin;
        pbnavitia::Journey* pb_journey = pb_response.add_journeys();

        pb_journey->set_nb_transfers(path.nb_changes);
        pb_journey->set_requested_date_time(navitia::to_posix_timestamp(path.request_time));

        // First streetnetwork item if it starts with coordinates
        if(!path.items.empty() && !path.items.front().stop_points.empty()) {
            const auto& departure_stop_point = path.items.front().stop_points.front();
            // for stop areas, we don't want to display the fallback section if start
            // from one of the stop area's stop point
            if (origin.uri != departure_stop_point->stop_area->uri) {
                auto temp = worker.get_path(departure_stop_point->idx);
                if(temp.path_items.size() > 0) {
                    //because of projection problem, the walking path might not join exactly the routing one
                    nt::GeographicalCoord routing_first_coord = departure_stop_point->coord;
                    if (temp.path_items.back().coordinates.back() != routing_first_coord) {
                        //if it's the case, we artificialy add the missing segment
                        temp.path_items.back().coordinates.push_back(routing_first_coord);
                    }

                    const auto walking_time = temp.duration;
                    departure_time = path.items.front().departure - walking_time.to_posix();
                    fill_street_sections(enhanced_response, origin, temp, d, pb_journey, departure_time);
                }
            }
        }

        const type::VehicleJourney* vj(nullptr);
        size_t item_idx(0);
        // La partie TC et correspondances
        for(auto path_i = path.items.begin(); path_i < path.items.end(); ++path_i) {
            const auto item = *path_i;

            pbnavitia::Section* pb_section = pb_journey->add_sections();
            pb_section->set_id(enhanced_response.register_section(pb_journey, item, item_idx++));

            if(item.type == public_transport) {
                pb_section->set_type(pbnavitia::PUBLIC_TRANSPORT);
                bt::ptime departure_ptime, arrival_ptime;
                vj = item.get_vj();
                int length = 0;
                for(size_t i=0;i<item.stop_points.size();++i) {
                    if (vj->has_boarding() || vj->has_landing()) {
                        continue;
                    }
                    pbnavitia::StopDateTime* stop_time = pb_section->add_stop_date_times();
                    auto arr_time = navitia::to_posix_timestamp(item.arrivals[i]);
                    stop_time->set_arrival_date_time(arr_time);
                    auto dep_time = navitia::to_posix_timestamp(item.departures[i]);
                    stop_time->set_departure_date_time(dep_time);
                    const auto p_deptime = item.departures[i];
                    const auto p_arrtime = item.arrivals[i];
                    bt::time_period action_period(p_deptime, p_arrtime);
                    fill_pb_object(item.stop_points[i], d, stop_time->mutable_stop_point(),
                            0, now, action_period, show_codes);

                    if (item.get_vj() != nullptr) {
                        vj = item.get_vj();
                        fill_pb_object(item.stop_times[i], d, stop_time, 1, now, action_period);
                    }

                    // L'heure de départ du véhicule au premier stop point
                    if(departure_ptime.is_not_a_date_time())
                        departure_ptime = p_deptime;
                    // L'heure d'arrivée au dernier stop point
                    arrival_ptime = p_arrtime;
                    if(i>0) {
                        const auto & previous_coord = item.stop_points[i-1]->coord;
                        const auto & current_coord = item.stop_points[i]->coord;
                        length += previous_coord.distance_to(current_coord);
                    }
                }
                if (item.stop_points.size() > 1) {
                    auto arr_time = item.arrivals[0];
                    auto dep_time = item.departures[0];
                    bt::time_period action_period(dep_time, arr_time);
                    // On the first item, if the type of the origin point is a stop area
                    // we want to display a stop area, otherwise a stop point
                    if(path_i == path.items.begin() && origin.type == nt::Type_e::StopArea
                            && origin.uri == item.stop_points.front()->stop_area->uri) {
                        fill_pb_placemark(item.stop_points.front()->stop_area, d,
                                pb_section->mutable_origin(), 1, now, action_period,
                                show_codes);
                    } else {
                        fill_pb_placemark(item.stop_points.front(), d,
                                pb_section->mutable_origin(), 1, now, action_period,
                                show_codes);
                    }
                    if(path_i == path.items.end()-1 && destination.type == nt::Type_e::StopArea
                            && destination.uri == item.stop_points.back()->stop_area->uri) {
                        fill_pb_placemark(item.stop_points.back()->stop_area, d,
                                pb_section->mutable_destination(), 1, now,
                                action_period, show_codes);
                    } else {
                        fill_pb_placemark(item.stop_points.back(), d,
                                pb_section->mutable_destination(), 1, now,
                                action_period, show_codes);
                    }
                }
                pb_section->set_length(length);
                if( item.get_vj() != nullptr) { // TODO : réfléchir si ça peut vraiment arriver
                    bt::time_period action_period(departure_ptime, arrival_ptime);
                    fill_section(pb_section, item.get_vj(), d, now, action_period);
                }
            } else {
                pb_section->set_type(pbnavitia::TRANSFER);
                switch(item.type) {
                    case stay_in : pb_section->set_transfer_type(pbnavitia::stay_in); break;
                    case guarantee : pb_section->set_transfer_type(pbnavitia::guaranteed); break;
                    case waiting : pb_section->set_type(pbnavitia::WAITING); break;
                    default : pb_section->set_transfer_type(pbnavitia::walking); break;
                }

                bt::time_period action_period(item.departure, item.arrival);
                const auto origin_sp = item.stop_points.front();
                const auto destination_sp = item.stop_points.back();
                fill_pb_placemark(origin_sp, d, pb_section->mutable_origin(), 1,
                        now, action_period, show_codes);
                fill_pb_placemark(destination_sp, d, pb_section->mutable_destination(), 1,
                        now, action_period, show_codes);
                pb_section->set_length(origin_sp->coord.distance_to(destination_sp->coord));
            }
            auto dep_time = navitia::to_posix_timestamp(item.departure);
            pb_section->set_begin_date_time(dep_time);
            auto arr_time = navitia::to_posix_timestamp(item.arrival);
            pb_section->set_end_date_time(arr_time);

            if(departure_time == bt::pos_infin)
                departure_time = item.departure;
            arrival_time = item.arrival;
            pb_section->set_duration((item.arrival - item.departure).total_seconds());
        }
        // La marche à pied finale si on avait donné une coordonnée
        if(!path.items.empty() && !path.items.back().stop_points.empty()) {
            const auto& arrival_stop_point = path.items.back().stop_points.back();
            // for stop areas, we don't want to display the fallback section if start
            // from one of the stop area's stop point
            if (destination.uri != arrival_stop_point->stop_area->uri) {
                auto temp = worker.get_path(arrival_stop_point->idx, true);
                if(temp.path_items.size() > 0) {
                   //add a junction between the routing path and the walking one if needed
                    nt::GeographicalCoord routing_last_coord = arrival_stop_point->coord;
                    if (temp.path_items.front().coordinates.front() != routing_last_coord) {
                        temp.path_items.front().coordinates.push_front(routing_last_coord);
                    }

                    auto begin_section_time = arrival_time;
                    fill_street_sections(enhanced_response, destination, temp, d, pb_journey,
                            begin_section_time);
                    arrival_time = arrival_time + temp.duration.to_posix();
                }
            }
        }

        const auto str_departure = navitia::to_posix_timestamp(departure_time);
        const auto str_arrival = navitia::to_posix_timestamp(arrival_time);
        pb_journey->set_departure_date_time(str_departure);
        pb_journey->set_arrival_date_time(str_arrival);
        pb_journey->set_duration((arrival_time - departure_time).total_seconds());

        //fare computation, done at the end for the journey to be complete
        auto fare = d.fare->compute_fare(path);
        fill_fare_section(enhanced_response, pb_journey, fare);
    }

    if (pb_response.journeys().size() == 0) {
        fill_pb_error(pbnavitia::Error::no_solution, "no solution found for this journey",
        pb_response.mutable_error());
        pb_response.set_response_type(pbnavitia::NO_SOLUTION);
    }

    return pb_response;
}

std::vector<std::pair<type::idx_t, navitia::time_duration> >
get_stop_points( const type::EntryPoint &ep, const type::Data& data,
        georef::StreetNetwork & worker, bool use_second = false){
    std::vector<std::pair<type::idx_t, navitia::time_duration> > result;
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
    LOG4CPLUS_DEBUG(logger, "Searching nearest stop_point's from entry point : [" << ep.coordinates.lat()
              << "," << ep.coordinates.lon() << "]");
    if(ep.type == type::Type_e::Address
                || ep.type == type::Type_e::Coord
                || ep.type == type::Type_e::StopArea
                || ep.type == type::Type_e::POI){
        std::set<type::idx_t> stop_points;
        if(ep.type == type::Type_e::StopArea){
            auto it = data.pt_data->stop_areas_map.find(ep.uri);
            if(it!= data.pt_data->stop_areas_map.end()) {
                for(auto stop_point : it->second->stop_point_list) {
                    if(stop_points.find(stop_point->idx) == stop_points.end()) {
                        result.push_back({stop_point->idx, {}});
                        stop_points.insert(stop_point->idx);
                    }
                }
            }
        }
        auto tmp_sn = worker.find_nearest_stop_points(
                    ep.streetnetwork_params.max_duration,
                    data.pt_data->stop_point_proximity_list,
                    use_second);
        for(auto idx_duration : tmp_sn) {
            auto sp_idx = idx_duration.first;
            if(stop_points.find(sp_idx) == stop_points.end()) {
                stop_points.insert(sp_idx);
                result.push_back(idx_duration);
            }
        }
    } else if(ep.type == type::Type_e::StopPoint) {
        auto it = data.pt_data->stop_points_map.find(ep.uri);
        if(it != data.pt_data->stop_points_map.end()){
            result.push_back({it->second->idx, {}});
        }
    } else if(ep.type == type::Type_e::Admin) {
        //for an admin, we want to leave from it's main stop areas if we have some, else we'll leave from the center of the admin
        auto it_admin = data.geo_ref->admin_map.find(ep.uri);
        if (it_admin == data.geo_ref->admin_map.end()) {
            LOG4CPLUS_ERROR(logger, "impossible to find admin " << ep.uri);
            return result;
        }
        const auto admin = data.geo_ref->admins[it_admin->second];

        if (! admin->main_stop_areas.empty()) {
            for (auto stop_area: admin->main_stop_areas) {
                for(auto stop_point : stop_area->stop_point_list) {
                    result.push_back({stop_point->idx, {}});
                }
            }
        } else {
            //we only add the center of the admin, and look for the stop points around
            result = worker.find_nearest_stop_points(
                        ep.streetnetwork_params.max_duration,
                        data.pt_data->stop_point_proximity_list,
                        use_second);
        }
        LOG4CPLUS_ERROR(logger, result.size() << " sp found for admin");
    }

    return result;
}


std::vector<bt::ptime>
parse_datetimes(RAPTOR &raptor,const std::vector<uint32_t>& timestamps,
                pbnavitia::Response &response, bool clockwise) {
    std::vector<bt::ptime> datetimes;

    for(uint32_t datetime: timestamps){
        bt::ptime ptime = bt::from_time_t(datetime);
        if(!raptor.data.meta->production_date.contains(ptime.date())) {
            fill_pb_error(pbnavitia::Error::date_out_of_bounds,
                            "date is not in data production period",
                            response.mutable_error());
            response.set_response_type(pbnavitia::DATE_OUT_OF_BOUNDS);
        }
        datetimes.push_back(ptime);
    }
    if(clockwise)
        std::sort(datetimes.begin(), datetimes.end(),
                  [](bt::ptime dt1, bt::ptime dt2){return dt1 > dt2;});
    else
        std::sort(datetimes.begin(), datetimes.end());

    return datetimes;
}

pbnavitia::Response
make_response(RAPTOR &raptor, const type::EntryPoint &origin,
              const type::EntryPoint &destination,
              const std::vector<uint32_t> &datetimes_str, bool clockwise,
              const type::AccessibiliteParams & accessibilite_params,
              std::vector<std::string> forbidden,
              georef::StreetNetwork & worker,
              bool disruption_active,
              bool allow_odt,
              uint32_t max_duration, uint32_t max_transfers, bool show_codes) {

    pbnavitia::Response response;

    std::vector<bt::ptime> datetimes;
    datetimes = parse_datetimes(raptor, datetimes_str, response, clockwise);
    if(response.has_error() || response.response_type() == pbnavitia::DATE_OUT_OF_BOUNDS) {
        return response;
    }
    worker.init(origin, {destination});
    auto departures = get_stop_points(origin, raptor.data, worker);
    auto destinations = get_stop_points(destination, raptor.data, worker, true);
    if(departures.size() == 0 && destinations.size() == 0){
        fill_pb_error(pbnavitia::Error::no_origin_nor_destionation, "no origin point nor destination point",response.mutable_error());
        response.set_response_type(pbnavitia::NO_ORIGIN_NOR_DESTINATION_POINT);
        return response;
    }

    if(departures.size() == 0){
        fill_pb_error(pbnavitia::Error::no_origin, "no origin point",response.mutable_error());
        response.set_response_type(pbnavitia::NO_ORIGIN_POINT);
        return response;
    }

    if(destinations.size() == 0){
        fill_pb_error(pbnavitia::Error::no_destination, "no destination point",response.mutable_error());
        response.set_response_type(pbnavitia::NO_DESTINATION_POINT);
        return response;
    }

    std::vector<Path> result;

    DateTime bound = clockwise ? DateTimeUtils::inf : DateTimeUtils::min;

    for(bt::ptime datetime : datetimes) {
        int day = (datetime.date() - raptor.data.meta->production_date.begin()).days();
        int time = datetime.time_of_day().total_seconds();
        DateTime init_dt = DateTimeUtils::set(day, time);

        if(max_duration!=std::numeric_limits<uint32_t>::max()) {
            bound = clockwise ? init_dt + max_duration : init_dt - max_duration;
        }

        std::vector<Path> tmp = raptor.compute_all(departures, destinations, init_dt, disruption_active, allow_odt, bound, max_transfers, accessibilite_params, forbidden, clockwise);

        // Lorsqu'on demande qu'un seul horaire, on garde tous les résultas
        if(datetimes.size() == 1) {
            result = tmp;
            for(auto & path : result) {
                path.request_time = datetime;
            }
        } else if(!tmp.empty()) {
            // Lorsqu'on demande plusieurs horaires, on garde que l'arrivée au plus tôt / départ au plus tard
            tmp.back().request_time = datetime;
            result.push_back(tmp.back());
            bound = to_datetime(tmp.back().items.back().arrival, raptor.data);
        } else // Lorsqu'on demande plusieurs horaires, et qu'il n'y a pas de résultat, on retourne un itinéraire vide
            result.push_back(Path());
    }
    if(clockwise)
        std::reverse(result.begin(), result.end());

    return make_pathes(result, raptor.data, worker, origin, destination, datetimes, clockwise, show_codes);
}


pbnavitia::Response make_isochrone(RAPTOR &raptor,
                                   type::EntryPoint origin,
                                   const uint32_t datetime_timestamp,bool clockwise,
                                   const type::AccessibiliteParams & accessibilite_params,
                                   std::vector<std::string> forbidden,
                                   georef::StreetNetwork & worker,
                                   bool disruption_active,
                                   bool allow_odt,
                                   int max_duration, uint32_t max_transfers, bool show_codes) {
    pbnavitia::Response response;

    bt::ptime datetime;
    auto tmp_datetime = parse_datetimes(raptor, {datetime_timestamp}, response, clockwise);
    if(response.has_error() || tmp_datetime.size() == 0 ||
       response.response_type() == pbnavitia::DATE_OUT_OF_BOUNDS) {
        return response;
    }
    datetime = tmp_datetime.front();
    worker.init(origin);
    auto departures = get_stop_points(origin, raptor.data, worker);

    if(departures.size() == 0){
        response.set_response_type(pbnavitia::NO_ORIGIN_POINT);
        return response;
    }

    int day = (datetime.date() - raptor.data.meta->production_date.begin()).days();
    int time = datetime.time_of_day().total_seconds();
    DateTime init_dt = DateTimeUtils::set(day, time);
    DateTime bound = clockwise ? init_dt + max_duration : init_dt - max_duration;

    raptor.isochrone(departures, init_dt, bound, max_transfers,
                           accessibilite_params, forbidden, clockwise, disruption_active, allow_odt);


    bt::ptime now = bt::second_clock::local_time();
    for(const type::StopPoint* sp : raptor.data.pt_data->stop_points) {
        DateTime best = bound;
        type::idx_t best_jpp = type::invalid_idx;
        int best_round = -1;
        for(auto jpp : sp->journey_pattern_point_list) {
            if(raptor.best_labels[jpp->idx] < best) {
                int round = raptor.best_round(jpp->idx);
                if(round != -1 && raptor.labels[round][jpp->idx].type == boarding_type::vj) {
                    best = raptor.best_labels[jpp->idx];
                    best_jpp = jpp->idx;
                    best_round = round;
                }
            }
        }

        if(best_jpp != type::invalid_idx) {
            auto label = raptor.best_labels[best_jpp];
            type::idx_t initial_jpp;
            DateTime initial_dt;
            boost::tie(initial_jpp, initial_dt) = get_final_jppidx_and_date(best_round,
                    best_jpp, clockwise, disruption_active, accessibilite_params, raptor);

            int duration = ::abs(label - init_dt);

            if(duration <= max_duration) {
                auto pb_journey = response.add_journeys();
                const auto str_departure = to_posix_timestamp(label, raptor.data);
                const auto str_arrival = to_posix_timestamp(label, raptor.data);
                const auto str_requested = to_posix_timestamp(init_dt, raptor.data);
                pb_journey->set_arrival_date_time(str_arrival);
                pb_journey->set_departure_date_time(str_departure);
                pb_journey->set_requested_date_time(str_requested);
                pb_journey->set_duration(duration);
                pb_journey->set_nb_transfers(best_round);
                bt::time_period action_period(navitia::to_posix_time(label-duration, raptor.data),
                        navitia::to_posix_time(label, raptor.data));
                fill_pb_placemark(raptor.data.pt_data->journey_pattern_points[best_jpp]->stop_point,
                        raptor.data, pb_journey->mutable_destination(), 0, now, action_period, show_codes);
            }
        }
    }

     std::sort(response.mutable_journeys()->begin(), response.mutable_journeys()->end(),
               [](const pbnavitia::Journey & journey1, const pbnavitia::Journey & journey2) {
               return journey1.duration() < journey2.duration();
                });


    return response;
}

}}
