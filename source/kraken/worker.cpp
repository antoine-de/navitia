#include "worker.h"

#include "utils/configuration.h"
#include "routing/raptor_api.h"
#include "autocomplete/autocomplete_api.h"
#include "proximity_list/proximitylist_api.h"
#include "ptreferential/ptreferential_api.h"
#include "time_tables/route_schedules.h"
#include "time_tables/next_passages.h"
#include "time_tables/2stops_schedules.h"
#include "time_tables/departure_boards.h"

namespace nt = navitia::type;
namespace pt = boost::posix_time;
namespace bg = boost::gregorian;

namespace navitia {

nt::Type_e get_type(pbnavitia::NavitiaType pb_type){
    switch(pb_type){
    case pbnavitia::ADDRESS: return nt::Type_e::Address; break;
    case pbnavitia::STOP_AREA: return nt::Type_e::StopArea; break;
    case pbnavitia::STOP_POINT: return nt::Type_e::StopPoint; break;
    case pbnavitia::LINE: return nt::Type_e::Line; break;
    case pbnavitia::ROUTE: return nt::Type_e::Route; break;
    case pbnavitia::JOURNEY_PATTERN: return nt::Type_e::JourneyPattern; break;
    case pbnavitia::NETWORK: return nt::Type_e::Network; break;
    case pbnavitia::COMMERCIAL_MODE: return nt::Type_e::CommercialMode; break;
    case pbnavitia::PHYSICAL_MODE: return nt::Type_e::PhysicalMode; break;
    case pbnavitia::CONNECTION: return nt::Type_e::Connection; break;
    case pbnavitia::JOURNEY_PATTERN_POINT: return nt::Type_e::JourneyPatternPoint; break;
    case pbnavitia::COMPANY: return nt::Type_e::Company; break;
    case pbnavitia::VEHICLE_JOURNEY: return nt::Type_e::VehicleJourney; break;
    case pbnavitia::POI: return nt::Type_e::POI; break;
    case pbnavitia::POITYPE: return nt::Type_e::POIType; break;
    case pbnavitia::ADMINISTRATIVE_REGION: return nt::Type_e::Admin; break;
    case pbnavitia::CALENDAR: return nt::Type_e::Calendar; break;
    default: return nt::Type_e::Unknown;
    }
}

template<class T>
std::vector<nt::Type_e> vector_of_pb_types(const T & pb_object){
    std::vector<nt::Type_e> result;
    for(int i = 0; i < pb_object.types_size(); ++i){
        result.push_back(get_type(pb_object.types(i)));
    }
    return result;
}

template<class T>
std::vector<std::string> vector_of_admins(const T & admin){
    std::vector<std::string> result;
    for (int i = 0; i < admin.admin_uris_size(); ++i){
        result.push_back(admin.admin_uris(i));
    }
    return result;
}

pbnavitia::Response Worker::status() {
    pbnavitia::Response result;

    auto status = result.mutable_status();
    const auto d = *data;
    boost::shared_lock<boost::shared_mutex> lock(d->load_mutex);
    status->set_publication_date(pt::to_iso_string(d->meta.publication_date));
    status->set_start_production_date(bg::to_iso_string(d->meta.production_date.begin()));
    status->set_end_production_date(bg::to_iso_string(d->meta.production_date.end()));
    status->set_data_version(d->version);
    status->set_navimake_version(d->meta.navimake_version);
    status->set_navitia_version(KRAKEN_VERSION);
    status->set_loaded(d->loaded);
    status->set_last_load_status(d->last_load);
    status->set_last_load_at(pt::to_iso_string(d->last_load_at));
    status->set_nb_threads(d->nb_threads);
    status->set_is_connected_to_rabbitmq(d->is_connected_to_rabbitmq);
    for(auto data_sources: d->meta.data_sources){
        status->add_data_sources(data_sources);
    }
    return result;
}

pbnavitia::Response Worker::metadatas() {
    pbnavitia::Response result;
    auto metadatas = result.mutable_metadatas();
    boost::shared_lock<boost::shared_mutex> lock((*data)->load_mutex);
    const auto d = *data;
    metadatas->set_start_production_date(bg::to_iso_string(d->meta.production_date.begin()));
    metadatas->set_end_production_date(bg::to_iso_string(d->meta.production_date.end()));
    metadatas->set_shape(d->meta.shape);
    metadatas->set_status("running");
    for(const type::Contributor* contributor : d->pt_data.contributors){
        metadatas->add_contributors(contributor->uri);
    }
    return result;
}

void Worker::init_worker_data(){
    if((*this->data)->last_load_at != this->last_load_at || !planner){
        planner = std::unique_ptr<routing::RAPTOR>(new routing::RAPTOR(*(*this->data)));
        street_network_worker = std::unique_ptr<georef::StreetNetwork>(new georef::StreetNetwork((*this->data)->geo_ref));
        this->last_load_at = (*this->data)->last_load_at;

        LOG4CPLUS_INFO(logger, "instanciation du planner");
    }
}


pbnavitia::Response Worker::autocomplete(const pbnavitia::PlacesRequest & request) {
    boost::shared_lock<boost::shared_mutex> lock((*data)->load_mutex);
    return navitia::autocomplete::autocomplete(request.q(),
            vector_of_pb_types(request), request.depth(), request.count(),
            vector_of_admins(request), request.search_type(), *(*this->data));
}

pbnavitia::Response Worker::disruptions(const pbnavitia::DisruptionsRequest &request){
    boost::shared_lock<boost::shared_mutex> lock((*data)->load_mutex);
    std::vector<std::string> forbidden_uris;
    for(int i = 0; i < request.forbidden_uris_size(); ++i)
        forbidden_uris.push_back(request.forbidden_uris(i));
    return navitia::disruption::disruptions(*(*this->data),
                                                request.datetime(),
                                                request.period(),
                                                request.depth(),
                                                request.count(),
                                                request.start_page(),
                                                request.filter(),
                                                forbidden_uris);
}

pbnavitia::Response Worker::calendars(const pbnavitia::CalendarsRequest &request){
    boost::shared_lock<boost::shared_mutex> lock((*data)->load_mutex);
    std::vector<std::string> forbidden_uris;
    for(int i = 0; i < request.forbidden_uris_size(); ++i)
        forbidden_uris.push_back(request.forbidden_uris(i));
    return navitia::calendar::calendars(*(*this->data),
                                        request.start_date(),
                                        request.end_date(),
                                        request.depth(),
                                        request.count(),
                                        request.start_page(),
                                        request.filter(),
                                        forbidden_uris);
}

pbnavitia::Response Worker::next_stop_times(const pbnavitia::NextStopTimeRequest &request,
        pbnavitia::API api) {
    boost::shared_lock<boost::shared_mutex> lock((*data)->load_mutex);
    int32_t max_date_times = request.has_max_date_times() ? request.max_date_times() : std::numeric_limits<int>::max();
    std::vector<std::string> forbidden_uri;
    for(int i = 0; i < request.forbidden_uri_size(); ++i)
        forbidden_uri.push_back(request.forbidden_uri(i));
    this->init_worker_data();
    try {
        switch(api) {
        case pbnavitia::NEXT_DEPARTURES:
            return timetables::next_departures(request.departure_filter(),
                    forbidden_uri, request.from_datetime(),
                    request.duration(), request.nb_stoptimes(), request.depth(),
                    type::AccessibiliteParams(), *(*this->data), false, request.count(),
                    request.start_page());
        case pbnavitia::NEXT_ARRIVALS:
            return timetables::next_arrivals(request.arrival_filter(),
                    forbidden_uri, request.from_datetime(),
                    request.duration(), request.nb_stoptimes(), request.depth(),
                    type::AccessibiliteParams(),
                    *(*this->data), false, request.count(), request.start_page());
        case pbnavitia::STOPS_SCHEDULES:
            return timetables::stops_schedule(request.departure_filter(),
                                              request.arrival_filter(),
                                              forbidden_uri,
                    request.from_datetime(), request.duration(), request.depth(),
                    *(*this->data), false);
        case pbnavitia::DEPARTURE_BOARDS:
            return timetables::departure_board(request.departure_filter(),
                    request.has_calendar() ? boost::optional<const std::string>(request.calendar()) : boost::optional<const std::string>(),
                    forbidden_uri, request.from_datetime(),
                    request.duration(),max_date_times, request.interface_version(),
                    request.count(), request.start_page(), *(*this->data), false);
        case pbnavitia::ROUTE_SCHEDULES:
            return timetables::route_schedule(request.departure_filter(),
                    forbidden_uri, request.from_datetime(),
                    request.duration(), request.interface_version(), request.depth(),
                    request.count(), request.start_page(), *(*this->data), false);
        default:
            LOG4CPLUS_WARN(logger, "Unknown timetable query");
            pbnavitia::Response response;
            fill_pb_error(pbnavitia::Error::unknown_api, "Unknown time table api",
                    response.mutable_error());
            return response;
        }

    } catch (const navitia::ptref::parsing_error& error) {
        LOG4CPLUS_ERROR(logger, "Error in the ptref request  : "+ error.more);
        pbnavitia::Response response;
        const auto str_error = "Unknow filter : " + error.more;
        fill_pb_error(pbnavitia::Error::bad_filter, str_error, response.mutable_error());
        return response;
    }
}


pbnavitia::Response Worker::proximity_list(const pbnavitia::PlacesNearbyRequest &request) {
    boost::shared_lock<boost::shared_mutex> lock((*data)->load_mutex);
    type::EntryPoint ep((*data)->get_type_of_id(request.uri()), request.uri());
    auto coord = this->coord_of_entry_point(ep);
    return proximitylist::find(coord, request.distance(), vector_of_pb_types(request),
                request.filter(), request.depth(), request.count(),
                request.start_page(), *(*this->data));
}


type::GeographicalCoord Worker::coord_of_entry_point(const type::EntryPoint & entry_point) {
    type::GeographicalCoord result;
    if(entry_point.type == Type_e::Address){
        auto way = (*this->data)->geo_ref.way_map.find(entry_point.uri);
        if (way != (*this->data)->geo_ref.way_map.end()){
            const auto geo_way = (*this->data)->geo_ref.ways[way->second];
            result = geo_way->nearest_coord(entry_point.house_number, (*this->data)->geo_ref.graph);
        }
    } else if (entry_point.type == Type_e::StopPoint) {
        auto sp_it = (*this->data)->pt_data.stop_points_map.find(entry_point.uri);
        if(sp_it != (*this->data)->pt_data.stop_points_map.end()) {
            result = sp_it->second->coord;
        }
    } else if (entry_point.type == Type_e::StopArea) {
           auto sa_it = (*this->data)->pt_data.stop_areas_map.find(entry_point.uri);
           if(sa_it != (*this->data)->pt_data.stop_areas_map.end()) {
               result = sa_it->second->coord;
           }
    } else if (entry_point.type == Type_e::Coord) {
        result = entry_point.coordinates;
    } else if (entry_point.type == Type_e::Admin) {
        auto it_admin = (*this->data)->geo_ref.admin_map.find(entry_point.uri);
        if (it_admin != (*this->data)->geo_ref.admin_map.end()) {
            const auto admin = (*this->data)->geo_ref.admins[it_admin->second];
            result = admin->coord;
        }

    } else if(entry_point.type == Type_e::POI){
        auto poi = (*this->data)->geo_ref.poi_map.find(entry_point.uri);
        if (poi != (*this->data)->geo_ref.poi_map.end()){
            const auto geo_poi = (*this->data)->geo_ref.pois[poi->second];
            result = geo_poi->coord;
        }
    }
    return result;
}


type::StreetNetworkParams Worker::streetnetwork_params_of_entry_point(const pbnavitia::StreetNetworkParams & request, const bool use_second){
    type::StreetNetworkParams result;
    std::string uri;

    if(use_second){
        result.mode = type::static_data::get()->modeByCaption(request.origin_mode());
        result.set_filter(request.origin_filter());
    }else{
        result.mode = type::static_data::get()->modeByCaption(request.destination_mode());
        result.set_filter(request.destination_filter());
    }
    switch(result.mode){
        case type::Mode_e::Bike:
            result.offset = (*data)->geo_ref.offsets[type::Mode_e::Bike];
            result.speed_factor = request.bike_speed() / georef::default_speed[type::Mode_e::Bike];
            break;
        case type::Mode_e::Car:
            result.offset = (*data)->geo_ref.offsets[type::Mode_e::Car];
            result.speed_factor = request.car_speed() / georef::default_speed[type::Mode_e::Car];
            break;
        case type::Mode_e::Bss:
            result.offset = (*data)->geo_ref.offsets[type::Mode_e::Bss];
            result.speed_factor = request.bss_speed() / georef::default_speed[type::Mode_e::Bss];
            break;
        default:
            result.offset = (*data)->geo_ref.offsets[type::Mode_e::Walking];
            result.speed_factor = request.walking_speed() / georef::default_speed[type::Mode_e::Walking];
            break;
    }
    int max_non_pt = request.max_duration_to_pt();
    result.max_duration = boost::posix_time::seconds(max_non_pt);
    return result;
}


pbnavitia::Response Worker::place_uri(const pbnavitia::PlaceUriRequest &request) {
    boost::shared_lock<boost::shared_mutex> lock((*data)->load_mutex);
    this->init_worker_data();
    pbnavitia::Response pb_response;

    if(request.uri().size() > 6 && request.uri().substr(0, 6) == "coord:") {
        type::EntryPoint ep(type::Type_e::Coord, request.uri());
        auto coord = this->coord_of_entry_point(ep);
        auto tmp = proximitylist::find(coord, 100, {type::Type_e::Address}, "", 1, 1, 0, *(*this->data));
        if(tmp.places_nearby().size() == 1){
            auto place = pb_response.add_places();
            place->CopyFrom(tmp.places_nearby(0));
        }
        return pb_response;
    }
    auto it_sa = (*data)->pt_data.stop_areas_map.find(request.uri());
    if(it_sa != (*data)->pt_data.stop_areas_map.end()) {
        pbnavitia::Place* place = pb_response.add_places();
        fill_pb_object(it_sa->second, **data, place->mutable_stop_area(), 1);
        place->set_embedded_type(pbnavitia::STOP_AREA);
        place->set_name(place->stop_area().name());
        place->set_uri(place->stop_area().uri());
    } else {
        auto it_sp = (*data)->pt_data.stop_points_map.find(request.uri());
        if(it_sp != (*data)->pt_data.stop_points_map.end()) {
            pbnavitia::Place* place = pb_response.add_places();
            fill_pb_object(it_sp->second, **data, place->mutable_stop_point(), 1);
            place->set_embedded_type(pbnavitia::STOP_POINT);
            place->set_name(place->stop_point().name());
            place->set_uri(place->stop_point().uri());
        } else {
            auto it_poi = (*data)->geo_ref.poi_map.find(request.uri());
            if(it_poi != (*data)->geo_ref.poi_map.end()) {
                pbnavitia::Place* place = pb_response.add_places();
                fill_pb_object((*data)->geo_ref.pois[it_poi->second], **data,
                        place->mutable_poi(), 1);
                place->set_embedded_type(pbnavitia::POI);
                place->set_name(place->poi().name());
                place->set_uri(place->poi().uri());
            } else {
                auto it_admin = (*data)->geo_ref.admin_map.find(request.uri());
                if(it_admin != (*data)->geo_ref.admin_map.end()) {
                    pbnavitia::Place* place = pb_response.add_places();
                    fill_pb_object((*data)->geo_ref.admins[it_admin->second],
                            **data, place->mutable_administrative_region(), 1);
                    place->set_embedded_type(pbnavitia::ADMINISTRATIVE_REGION);
                    place->set_name(place->administrative_region().name());
                    place->set_uri(place->administrative_region().uri());
                }else{
                    fill_pb_error(pbnavitia::Error::unable_to_parse, "Unable to parse : "+request.uri(),  pb_response.mutable_error());
                }
            }
        }
    }
    return pb_response;
}

pbnavitia::Response Worker::journeys(const pbnavitia::JourneysRequest &request, pbnavitia::API api) {
    boost::shared_lock<boost::shared_mutex> lock((*data)->load_mutex);
    this->init_worker_data();

    std::vector<type::EntryPoint> origins;
    for(int i = 0; i < request.origin().size(); i++) {
        Type_e origin_type = (*data)->get_type_of_id(request.origin(i).place());
        type::EntryPoint origin = type::EntryPoint(origin_type, request.origin(i).place(), request.origin(i).access_duration());

        if (origin.type == type::Type_e::Address || origin.type == type::Type_e::Admin
                || origin.type == type::Type_e::StopArea || origin.type == type::Type_e::StopPoint
                || origin.type == type::Type_e::POI) {
            origin.coordinates = this->coord_of_entry_point(origin);
        }
        origins.push_back(origin);
    }

    std::vector<type::EntryPoint> destinations;
    if(api != pbnavitia::ISOCHRONE) {
        for(int i = 0; i < request.destination().size(); i++) {
            Type_e destination_type = (*data)->get_type_of_id(request.destination(i).place());
            type::EntryPoint destination = type::EntryPoint(destination_type, request.destination(i).place(), request.destination(i).access_duration());

            if (destination.type == type::Type_e::Address || destination.type == type::Type_e::Admin
                    || destination.type == type::Type_e::StopArea || destination.type == type::Type_e::StopPoint
                    || destination.type == type::Type_e::POI) {
                destination.coordinates = this->coord_of_entry_point(destination);
            }
            destinations.push_back(destination);
        }
    }

    std::vector<std::string> forbidden;
    for(int i = 0; i < request.forbidden_uris_size(); ++i)
        forbidden.push_back(request.forbidden_uris(i));

    std::vector<std::string> datetimes;
    for(int i = 0; i < request.datetimes_size(); ++i)
        datetimes.push_back(request.datetimes(i));

    /// Récupération des paramètres de rabattement au départ
    for(int i = 0; i < origins.size(); i++) {
        type::EntryPoint *origin = &origins[i];
        if ((origin->type == type::Type_e::Address) || (origin->type == type::Type_e::Coord)
                || (origin->type == type::Type_e::Admin) || (origin->type == type::Type_e::POI) || (origin->type == type::Type_e::StopArea)){
            origin->streetnetwork_params = this->streetnetwork_params_of_entry_point(request.streetnetwork_params());
        }
    }

    /// Récupération des paramètres de rabattement à l'arrivée
    for (int i = 0; i < destinations.size(); i++) {
        type::EntryPoint *destination = &destinations[i];
        if ((destination->type == type::Type_e::Address) || (destination->type == type::Type_e::Coord)
                || (destination->type == type::Type_e::Admin) || (destination->type == type::Type_e::POI) || (destination->type == type::Type_e::StopArea)){
            destination->streetnetwork_params = this->streetnetwork_params_of_entry_point(request.streetnetwork_params(), false);
        }
    }

    /// Accessibilité, il faut initialiser ce paramètre
    //HOT FIX degueulasse
    type::AccessibiliteParams accessibilite_params;
    accessibilite_params.properties.set(type::hasProperties::WHEELCHAIR_BOARDING, request.wheelchair());

    /// Dispatch à nouveau
    switch(api){
    case pbnavitia::ISOCHRONE:
        return navitia::routing::make_isochrone(*planner, origins[0], request.datetimes(0),
                request.clockwise(), accessibilite_params,
                forbidden, *street_network_worker,
                request.disruption_active(), request.max_duration(),
                request.max_transfers());
        break;

    case pbnavitia::NEMPLANNER:
        return routing::make_nem_response(*planner, origins, destinations, datetimes,
                request.clockwise(), request.details(), accessibilite_params,
                forbidden, *street_network_worker,
                request.disruption_active(), request.max_duration(),
                request.max_transfers());
        break;

    case pbnavitia::PLANNER:
        return routing::make_response(*planner, origins[0], destinations[0], datetimes,
                request.clockwise(), accessibilite_params,
                forbidden, *street_network_worker,
                request.disruption_active(), request.max_duration(),
                request.max_transfers());
        break;
    }
}


pbnavitia::Response Worker::pt_ref(const pbnavitia::PTRefRequest &request){
    boost::shared_lock<boost::shared_mutex> lock((*data)->load_mutex);
    std::vector<std::string> forbidden_uri;
    for(int i = 0; i < request.forbidden_uri_size(); ++i)
        forbidden_uri.push_back(request.forbidden_uri(i));
    return navitia::ptref::query_pb(get_type(request.requested_type()),
                                    request.filter(), forbidden_uri,
                                    request.depth(), request.start_page(),
                                    request.count(), *(*this->data));
}


pbnavitia::Response Worker::dispatch(const pbnavitia::Request& request) {
    pbnavitia::Response result ;
    if (! (*data)->loaded){
        fill_pb_error(pbnavitia::Error::service_unavailable, "The service is loading data", result.mutable_error());
        return result;
    }
    switch(request.requested_api()){
        case pbnavitia::STATUS: return status(); break;
        case pbnavitia::places: return autocomplete(request.places()); break;
        case pbnavitia::place_uri: return place_uri(request.place_uri()); break;
        case pbnavitia::ROUTE_SCHEDULES:
        case pbnavitia::NEXT_DEPARTURES:
        case pbnavitia::NEXT_ARRIVALS:
        case pbnavitia::STOPS_SCHEDULES:
        case pbnavitia::DEPARTURE_BOARDS:
            return next_stop_times(request.next_stop_times(), request.requested_api()); break;
        case pbnavitia::ISOCHRONE:
        case pbnavitia::NEMPLANNER:
        case pbnavitia::PLANNER: return journeys(request.journeys(), request.requested_api()); break;
        case pbnavitia::places_nearby: return proximity_list(request.places_nearby()); break;
        case pbnavitia::PTREFERENTIAL: return pt_ref(request.ptref()); break;
        case pbnavitia::METADATAS : return metadatas(); break;
        case pbnavitia::disruptions : return disruptions(request.disruptions()); break;
        case pbnavitia::calendars : return calendars(request.calendars()); break;
        default:
            LOG4CPLUS_WARN(logger, "Unknown API : " + API_Name(request.requested_api()));
            fill_pb_error(pbnavitia::Error::unknown_api, "Unknown API", result.mutable_error());
            break;
    }

    return result;
}

}
