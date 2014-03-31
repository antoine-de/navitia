#include "raptor_api.h"
#include "type/pb_converter.h"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "type/datetime.h"
#include <unordered_set>
#include <chrono>

namespace bt = boost::posix_time;

namespace navitia { namespace routing {

void fill_section(pbnavitia::Section *pb_section, const type::VehicleJourney* vj,
        const nt::Data & d, bt::ptime now,
        bt::time_period action_period) {

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
        const std::vector<bt::ptime>& datetimes,
                                bool clockwise) {
    EnhancedResponse enhanced_response; //wrapper around raw protobuff response to handle ids
    pbnavitia::Response& pb_response = enhanced_response.response;

    bt::ptime now = bt::second_clock::local_time();

    auto temp = worker.get_direct_path();
    if(!temp.path_items.empty()) {
        pb_response.set_response_type(pbnavitia::ITINERARY_FOUND);

        //for each date time we add a direct street journey
        for(bt::ptime datetime : datetimes) {
            pbnavitia::Journey* pb_journey = pb_response.add_journeys();
            pb_journey->set_duration(temp.duration.total_seconds());

            bt::ptime departure;
            if (clockwise) {
                departure = datetime;
            } else {
                departure = datetime - temp.duration;
            }
            fill_street_sections(enhanced_response, paths[0].origin, temp, d, pb_journey, departure);

            const auto str_departure = navitia::to_iso_string_no_fractional(departure);
            const auto str_arrival = navitia::to_iso_string_no_fractional(departure + temp.duration);
            pb_journey->set_departure_date_time(str_departure);
            pb_journey->set_arrival_date_time(str_arrival);
        }
    }

    pb_response.set_response_type(pbnavitia::ITINERARY_FOUND);

    for(Path path : paths) {
        bt::ptime departure_time = bt::pos_infin,
                          arrival_time = bt::pos_infin;
        pbnavitia::Journey* pb_journey = pb_response.add_journeys();

        pb_journey->set_nb_transfers(path.nb_changes);
        pb_journey->set_requested_date_time(navitia::to_iso_string_no_fractional(path.request_time));

        // La marche à pied initiale si on avait donné une coordonnée
        if(path.items.size() > 0 && ! path.items.front().stop_points.empty()) {
            auto temp = worker.get_path(path.items.front().stop_points.front()->idx);
            if(temp.path_items.size() > 0) {
                //because of projection problem, the walking path might not join exactly the routing one
                nt::GeographicalCoord routing_first_coord = path.items.front().stop_points.front()->coord;
                if (temp.path_items.back().coordinates.back() != routing_first_coord) {
                    //if it's the case, we artificialy add the missing segment
                    temp.path_items.back().coordinates.push_back(routing_first_coord);
                }

                const auto walking_time = temp.duration;
                departure_time = path.items.front().departure - walking_time;
                fill_street_sections(enhanced_response, path.origin, temp, d, pb_journey, departure_time);
            }
        }

        const type::VehicleJourney* vj = nullptr;
        size_t item_idx(0);
        // La partie TC et correspondances
        for(PathItem & item : path.items) {

            pbnavitia::Section* pb_section = pb_journey->add_sections();
            pb_section->set_id(enhanced_response.register_section(pb_journey, item, item_idx++));

            if(item.type == public_transport) {
                pb_section->set_type(pbnavitia::PUBLIC_TRANSPORT);
                bt::ptime departure_ptime , arrival_ptime;
                vj = item.get_vj();
                int length = 0;
                for(size_t i=0;i<item.stop_points.size();++i) {
                    if ((!vj->has_boarding()) && (!vj->has_landing())) {
                        pbnavitia::StopDateTime* stop_time = pb_section->add_stop_date_times();
                        auto arr_time = navitia::to_iso_string_no_fractional(item.arrivals[i]);
                        stop_time->set_arrival_date_time(arr_time);
                        auto dep_time = navitia::to_iso_string_no_fractional(item.departures[i]);
                        stop_time->set_departure_date_time(dep_time);
                        const auto p_deptime = item.departures[i];
                        const auto p_arrtime = item.arrivals[i];
                        bt::time_period action_period(p_deptime, p_arrtime);
                        fill_pb_object(item.stop_points[i], d, stop_time->mutable_stop_point(), 0, now, action_period);

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
                }
                if (item.stop_points.size() > 1) {
                    auto arr_time = item.arrivals[0];
                    auto dep_time = item.departures[0];
                    bt::time_period action_period(dep_time, arr_time);
                    fill_pb_placemark(item.stop_points.front(), d, pb_section->mutable_origin(), 1, now, action_period);
                    fill_pb_placemark(item.stop_points.back(), d, pb_section->mutable_destination(), 1, now, action_period);
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
                fill_pb_placemark(origin_sp, d, pb_section->mutable_origin(), 1, now, action_period);
                fill_pb_placemark(destination_sp, d, pb_section->mutable_destination(), 1, now, action_period);
                pb_section->set_length(origin_sp->coord.distance_to(destination_sp->coord));
            }
            auto dep_time = navitia::to_iso_string_no_fractional(item.departure);
            pb_section->set_begin_date_time(dep_time);
            auto arr_time = navitia::to_iso_string_no_fractional(item.arrival);
            pb_section->set_end_date_time(arr_time);

            if(departure_time == bt::pos_infin)
                departure_time = item.departure;
            arrival_time = item.arrival;
            pb_section->set_duration((item.arrival - item.departure).total_seconds());
        }
        // La marche à pied finale si on avait donné une coordonnée
        if(path.items.size() > 0 && ! path.items.back().stop_points.empty()) {
            auto temp = worker.get_path(path.items.back().stop_points.back()->idx, true);
            if(temp.path_items.size() > 0) {
               //add a junction between the routing path and the walking one if needed
                nt::GeographicalCoord routing_last_coord = path.items.back().stop_points.back()->coord;
                if (temp.path_items.front().coordinates.front() != routing_last_coord) {
                    temp.path_items.front().coordinates.push_front(routing_last_coord);
                }

                auto begin_section_time = arrival_time;
                fill_street_sections(enhanced_response, path.destination, temp, d, pb_journey, begin_section_time);
                arrival_time = arrival_time + temp.duration;
            }
        }

        const auto str_departure = navitia::to_iso_string_no_fractional(departure_time);
        const auto str_arrival = navitia::to_iso_string_no_fractional(arrival_time);
        pb_journey->set_departure_date_time(str_departure);
        pb_journey->set_arrival_date_time(str_arrival);
        pb_journey->set_duration((arrival_time - departure_time).total_seconds());

        //fare computation, done at the end for the journey to be complete
        auto fare = d.fare.compute_fare(path);
        fill_fare_section(enhanced_response, pb_journey, fare);
    }

	if (pb_response.journeys().size() == 0) {
        fill_pb_error(pbnavitia::Error::no_solution, "no solution found for this journey",
        pb_response.mutable_error());
        pb_response.set_response_type(pbnavitia::NO_SOLUTION);
    }

    return pb_response;
}


std::vector<std::pair<type::idx_t, bt::time_duration> >
get_stop_points( const type::EntryPoint &ep, const type::PT_Data & pt_data,
        georef::StreetNetwork & worker, bool use_second = false){
    std::vector<std::pair<type::idx_t, bt::time_duration> > result;
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
    LOG4CPLUS_DEBUG(logger, "calcul des stop points pour l'entry point : [" << ep.coordinates.lat()
              << "," << ep.coordinates.lon() << "]");
    if(ep.type == type::Type_e::Address
                || ep.type == type::Type_e::Coord || ep.type == type::Type_e::Admin
                || ep.type == type::Type_e::StopArea){
        result = worker.find_nearest_stop_points(
                    ep.streetnetwork_params.max_duration,
                    pt_data.stop_point_proximity_list,
                    use_second);
        std::set<type::idx_t> stop_points;
        for(auto idx_duration : result) {
            stop_points.insert(idx_duration.first);
        }
        if(ep.type == type::Type_e::StopArea){
            auto it = pt_data.stop_areas_map.find(ep.uri);
            if(it!= pt_data.stop_areas_map.end()) {
                for(auto stop_point : it->second->stop_point_list) {
                    if(stop_points.find(stop_point->idx) == stop_points.end()) {
                        result.push_back({stop_point->idx, {}});
                    }
                }
            }
        }
    }else if(ep.type == type::Type_e::StopPoint){
        auto it = pt_data.stop_points_map.find(ep.uri);
        if(it != pt_data.stop_points_map.end()){
            result.push_back({it->second->idx, {}});
        }
    }

    return result;
}


std::vector<bt::ptime>
parse_datetimes(RAPTOR &raptor,const std::vector<std::string> &datetimes_str,
                pbnavitia::Response &response, bool clockwise) {
    std::vector<bt::ptime> datetimes;

    for(std::string datetime: datetimes_str){
        try {
            bt::ptime ptime;
            ptime = bt::from_iso_string(datetime);
            if(!raptor.data.meta.production_date.contains(ptime.date())) {
                fill_pb_error(pbnavitia::Error::date_out_of_bounds,
                                "date is not in data production period",
                                response.mutable_error());
                response.set_response_type(pbnavitia::DATE_OUT_OF_BOUNDS);
            }
            datetimes.push_back(ptime);
        } catch(...){
//            response.set_error("Impossible to parse date " + datetime);
            fill_pb_error(pbnavitia::Error::unable_to_parse, "Unable to parse Datetime",response.mutable_error());
            response.set_info("Example of invalid date: " + datetime);
        }
    }
    if(clockwise)
        std::sort(datetimes.begin(), datetimes.end(),
                  [](bt::ptime dt1, bt::ptime dt2){return dt1 > dt2;});
    else
        std::sort(datetimes.begin(), datetimes.end());

    return datetimes;
}

pbnavitia::Response
make_response(RAPTOR &raptor,
              const type::EntryPoint &origin,
              const type::EntryPoint &destination,
              const std::vector<std::string> &datetimes_str, bool clockwise,
              const type::AccessibiliteParams & accessibilite_params,
              std::vector<std::string> forbidden,
              georef::StreetNetwork & worker,
              bool disruption_active,
              uint32_t max_duration, uint32_t max_transfers) {

    return make_generic_response(raptor,
        {clockwise ? origin : destination}, {clockwise ? destination : origin},
        datetimes_str, clockwise, true /* details */,
        accessibilite_params,
        forbidden,
        worker, disruption_active, max_duration, max_transfers);
}

pbnavitia::Response
make_nem_response(RAPTOR &raptor,
                  const std::vector<type::EntryPoint> &origins,
                  const std::vector<type::EntryPoint> &destinations,
                  const std::vector<std::string> &datetimes_str,
                  bool clockwise, bool details,
                  const type::AccessibiliteParams & accessibilite_params,
                  std::vector<std::string> forbidden,
                  georef::StreetNetwork & worker,
                  bool disruption_active,
                  uint32_t max_duration, uint32_t max_transfers) {

    pbnavitia::Response response;
    int count = 0;

    std::vector<type::EntryPoint> calc_dep = clockwise ? origins : destinations;
    std::vector<type::EntryPoint> calc_dest = clockwise ? destinations : origins;

    for (int i = 0; i < calc_dest.size(); i++) {
        pbnavitia::Response response_i = make_generic_response(
                    raptor,
                    calc_dep, {calc_dest[i]}, datetimes_str,
                    clockwise, details,
                    accessibilite_params,
                    forbidden,
                    worker,
                    disruption_active,
                    max_duration,
                    max_transfers);

        response = response_i;

        // TO DO : join responses
        //pbnavitia::Journey *journey = response.add_journeys();
        //journey->ParseFromString(response_i.journeys(0).SerializeAsString());

        count++;
        break;
    }

    if (count == 0) {
        response.set_response_type(pbnavitia::NO_SOLUTION);
        response.mutable_error()->set_id(pbnavitia::Error_error_id_no_solution);
        response.mutable_error()->set_message("no solution found for this journey");
    }
    else
        response.set_response_type(pbnavitia::ITINERARY_FOUND);

    return response;
}

pbnavitia::Response
make_generic_response(RAPTOR &raptor,
              const std::vector<type::EntryPoint> &calc_dep,
              const std::vector<type::EntryPoint> &calc_arr,
              const std::vector<std::string> &datetimes_str, bool clockwise, bool details,
              const type::AccessibiliteParams & accessibilite_params,
              std::vector<std::string> forbidden,
              georef::StreetNetwork & worker,
              bool disruption_active,
              uint32_t max_duration, uint32_t max_transfers) {

    pbnavitia::Response response;

    std::vector<bt::ptime> datetimes;
    datetimes = parse_datetimes(raptor, datetimes_str, response, clockwise);
    if(response.has_error() || response.response_type() == pbnavitia::DATE_OUT_OF_BOUNDS) {
        return response;
    }
    worker.init(calc_dep[0], {calc_arr[0]});

    std::vector<std::pair<type::EntryPoint, std::vector<std::pair<type::idx_t, bt::time_duration> > > > multi_departures;
    int origin_count = 0;
    for (auto item_dep : calc_dep) {
        multi_departures.push_back(std::make_pair(item_dep, get_stop_points(item_dep, raptor.data.pt_data, worker)));
        origin_count += multi_departures.back().second.size();
    }

    std::vector<std::pair<type::EntryPoint, std::vector<std::pair<type::idx_t, bt::time_duration> > > > multi_arrivals;
    int destination_count = 0;
    for (auto item_arr : calc_arr) {
        multi_arrivals.push_back(std::make_pair(item_arr, get_stop_points(item_arr, raptor.data.pt_data, worker, true)));
        destination_count += multi_arrivals.back().second.size();
    }

    if(origin_count == 0 && destination_count == 0) {
        fill_pb_error(pbnavitia::Error::no_origin_nor_destination, "no origin point nor destination point",response.mutable_error());
        response.set_response_type(pbnavitia::NO_ORIGIN_NOR_DESTINATION_POINT);
        return response;
    }

    if((origin_count == 0 && clockwise) || (destination_count == 0 && !clockwise)) {
        fill_pb_error(pbnavitia::Error::no_origin, "no origin point",response.mutable_error());
        response.set_response_type(pbnavitia::NO_ORIGIN_POINT);
        return response;
    }

    if((destination_count == 0 && clockwise) || (origin_count == 0 && clockwise)) {
        fill_pb_error(pbnavitia::Error::no_destination, "no destination point",response.mutable_error());
        response.set_response_type(pbnavitia::NO_DESTINATION_POINT);
        return response;
    }

    std::vector<Path> result;

    DateTime bound = clockwise ? DateTimeUtils::inf : DateTimeUtils::min;

    for(bt::ptime datetime : datetimes) {
        int day = (datetime.date() - raptor.data.meta.production_date.begin()).days();
        int time = datetime.time_of_day().total_seconds();
        DateTime init_dt = DateTimeUtils::set(day, time);

        if(max_duration!=std::numeric_limits<uint32_t>::max()) {
            bound = clockwise ? init_dt + max_duration : init_dt - max_duration;
        }

        std::vector<Path> tmp = raptor.compute_all(multi_departures, multi_arrivals,
                                                   init_dt, disruption_active, bound, max_transfers,
                                                   accessibilite_params, forbidden,
                                                   clockwise, details);

        // Lorsqu'on demande qu'un seul horaire, on garde tous les résultats
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

    return make_pathes(result, raptor.data, worker, datetimes, clockwise);
}


pbnavitia::Response make_isochrone(RAPTOR &raptor,
                                   type::EntryPoint origin,
                                   const std::string &datetime_str,bool clockwise,
                                   const type::AccessibiliteParams & accessibilite_params,
                                   std::vector<std::string> forbidden,
                                   georef::StreetNetwork & worker,
                                   bool disruption_active,
                                   int max_duration, uint32_t max_transfers) {
    pbnavitia::Response response;

    bt::ptime datetime;
    auto tmp_datetime = parse_datetimes(raptor, {datetime_str}, response, clockwise);
    if(response.has_error() || tmp_datetime.size() == 0 ||
       response.response_type() == pbnavitia::DATE_OUT_OF_BOUNDS) {
        return response;
    }
    datetime = tmp_datetime.front();
    worker.init(origin);
    auto departures = get_stop_points(origin, raptor.data.pt_data, worker);

    if(departures.size() == 0){
        response.set_response_type(pbnavitia::NO_ORIGIN_POINT);
        return response;
    }

    int day = (datetime.date() - raptor.data.meta.production_date.begin()).days();
    int time = datetime.time_of_day().total_seconds();
    DateTime init_dt = DateTimeUtils::set(day, time);
    DateTime bound = clockwise ? init_dt + max_duration : init_dt - max_duration;

    raptor.isochrone(departures, init_dt, bound, max_transfers,
                           accessibilite_params/*wheelchair*/, forbidden, clockwise, disruption_active);


    for(const type::StopPoint* sp : raptor.data.pt_data.stop_points) {
        DateTime best = bound;
        type::idx_t best_rp = type::invalid_idx;
        int best_round = -1;
        for(auto jpp : sp->journey_pattern_point_list) {
            if(raptor.best_labels[jpp->idx] < best) {
                int round = raptor.best_round(jpp->idx);
                if(round != -1 && raptor.labels[round][jpp->idx].type == boarding_type::vj) {
                    best = raptor.best_labels[jpp->idx];
                    best_rp = jpp->idx;
                    best_round = round;
                }
            }
        }

        if(best_rp != type::invalid_idx) {
            auto label = raptor.best_labels[best_rp];
            type::idx_t initial_rp;
            DateTime initial_dt;
            boost::tie(initial_rp, initial_dt) = getFinalRpidAndDate(best_round, best_rp, clockwise, raptor.labels);

            int duration = ::abs(label - init_dt);

            if(duration <= max_duration) {
                auto pb_journey = response.add_journeys();
                const auto str_departure = iso_string(label, raptor.data);
                const auto str_arrival = iso_string(label, raptor.data);
                const auto str_requested = iso_string(init_dt, raptor.data);
                pb_journey->set_arrival_date_time(str_arrival);
                pb_journey->set_departure_date_time(str_departure);
                pb_journey->set_requested_date_time(str_requested);
                pb_journey->set_duration(duration);
                pb_journey->set_nb_transfers(best_round);
                fill_pb_placemark(raptor.data.pt_data.journey_pattern_points[best_rp]->stop_point, raptor.data, pb_journey->mutable_destination(), 0);
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
