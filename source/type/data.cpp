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

#include "data.h"

#include <fstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include "third_party/eos_portable_archive/portable_iarchive.hpp"
#include "third_party/eos_portable_archive/portable_oarchive.hpp"
#include "lz4_filter/filter.h"
#include "utils/functions.h"
#include "utils/exception.h"

#include "pt_data.h"
#include "routing/dataraptor.h"
#include "georef/georef.h"
#include "fare/fare.h"
#include "type/meta_data.h"

namespace pt = boost::posix_time;

namespace navitia { namespace type {

Data::Data() : meta(std::make_unique<MetaData>()),
        pt_data(std::make_unique<PT_Data>()),
        geo_ref(std::make_unique<navitia::georef::GeoRef>()),
        dataRaptor(std::make_unique<navitia::routing::dataRAPTOR>()),
        fare(std::make_unique<navitia::fare::Fare>()){
    this->is_connected_to_rabbitmq = false;
    this->loaded = false;
}

Data::~Data(){}

bool Data::load(const std::string & filename) {
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
    try {
        this->load_lz4(filename);
        this->build_raptor();
        last_load_at = pt::microsec_clock::local_time();
        last_load = true;
        loaded = true;
        LOG4CPLUS_INFO(logger, boost::format("Nb data stop times : %d stopTimes : %d nb foot path : %d Nombre de stop points : %d")
                % pt_data->stop_times.size() % dataRaptor->arrival_times.size()
                % dataRaptor->foot_path_forward.size() % pt_data->stop_points.size()
                );
    } catch(const wrong_version& ex) {
        LOG4CPLUS_ERROR(logger, boost::format("Data loading failed: %s") % ex.what());
        last_load = false;
    } catch(const std::exception& ex) {
        LOG4CPLUS_ERROR(logger, boost::format("le chargement des données à échoué: %s") % ex.what());
        last_load = false;
    } catch(...) {
        LOG4CPLUS_ERROR(logger, "le chargement des données à échoué");
        last_load = false;
    }
    return this->last_load;
}

void Data::load_lz4(const std::string & filename) {
    std::ifstream ifs(filename.c_str(),  std::ios::in | std::ios::binary);
    boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
    in.push(LZ4Decompressor(2048*500),8192*500, 8192*500);
    in.push(ifs);
    eos::portable_iarchive ia(in);
    ia >> *this;
}

void Data::save(const std::string & filename){
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
    this->save_lz4(filename);
}

void Data::save_lz4(const std::string & filename) {
    auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
    boost::filesystem::path p(filename);
    boost::filesystem::path dir = p.parent_path();
    try {
       boost::filesystem::is_directory(p);
    } catch(const boost::filesystem::filesystem_error& e)
    {
       if(e.code() == boost::system::errc::permission_denied)
           LOG4CPLUS_ERROR(logger, "Search permission is denied for " << p);
       else
           LOG4CPLUS_ERROR(logger, "is_directory(" << p << ") failed with "
                     << e.code().message());
       throw navitia::exception("Unable to write file");
    }
    try {
        std::ofstream ofs(filename.c_str(),std::ios::out|std::ios::binary|std::ios::trunc);
        boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
        out.push(LZ4Compressor(2048*500), 1024*500, 1024*500);
        out.push(ofs);
        eos::portable_oarchive oa(out);
        oa << *this;
    } catch(const boost::filesystem::filesystem_error &e) {
        if(e.code() == boost::system::errc::permission_denied)
            LOG4CPLUS_ERROR(logger, "Writing permission is denied for " << p);
        else if(e.code() == boost::system::errc::file_too_large)
            LOG4CPLUS_ERROR(logger, "The file " << filename << " is too large");
        else if(e.code() == boost::system::errc::interrupted)
            LOG4CPLUS_ERROR(logger, "Writing was interrupted for " << p);
        else if(e.code() == boost::system::errc::no_buffer_space)
            LOG4CPLUS_ERROR(logger, "No buffer space while writing " << p);
        else if(e.code() == boost::system::errc::not_enough_memory)
            LOG4CPLUS_ERROR(logger, "Not enough memory while writing " << p);
        else if(e.code() == boost::system::errc::no_space_on_device)
            LOG4CPLUS_ERROR(logger, "No space on device while writing " << p);
        else if(e.code() == boost::system::errc::operation_not_permitted)
            LOG4CPLUS_ERROR(logger, "Operation not permitted while writing " << p);
        LOG4CPLUS_ERROR(logger, e.what());
       throw navitia::exception("Unable to write file");
    }
}

void Data::build_uri(){
#define CLEAR_EXT_CODE(type_name, collection_name) this->pt_data->collection_name##_map.clear();
ITERATE_NAVITIA_PT_TYPES(CLEAR_EXT_CODE)
    this->pt_data->build_uri();
    geo_ref->build_pois_map();
    geo_ref->build_poitypes_map();
    geo_ref->build_admin_map();
}

void Data::build_proximity_list(){
    this->pt_data->build_proximity_list();
    this->geo_ref->build_proximity_list();
    this->geo_ref->project_stop_points(this->pt_data->stop_points);
}

void  Data::build_administrative_regions(){
    this->geo_ref->build_admins_stop_points(this->pt_data->stop_points);
    this->geo_ref->build_admins_pois();
    this->pt_data->build_admins_stop_areas();
}

void Data::build_autocomplete(){
    pt_data->build_autocomplete(*geo_ref);
    geo_ref->build_autocomplete_list();
    pt_data->compute_score_autocomplete(*geo_ref);
}

void Data::build_raptor() {
    dataRaptor->load(*this->pt_data);
}

ValidityPattern* Data::get_similar_validity_pattern(ValidityPattern* vp) const{
    auto find_vp_predicate = [&](ValidityPattern* vp1) { return ((*vp) == (*vp1));};
    auto it = std::find_if(this->pt_data->validity_patterns.begin(),
                        this->pt_data->validity_patterns.end(), find_vp_predicate);
    if(it != this->pt_data->validity_patterns.end()) {
        return *(it);
    } else {
        return nullptr;
    }
}

using list_cal_bitset = std::vector<std::pair<const Calendar*, ValidityPattern::year_bitset>>;

list_cal_bitset
find_matching_calendar(const Data&, const std::string& name, const ValidityPattern& validity_pattern,
                        const std::vector<Calendar*>& calendar_list, double relative_threshold) {
    list_cal_bitset res;
    //for the moment we keep lot's of trace, but they will be removed after a while
    auto log = log4cplus::Logger::getInstance("kraken::type::Data::Calendar");
    LOG4CPLUS_TRACE(log, "meta vj " << name << " :" << validity_pattern.days.to_string());

    for (const auto calendar : calendar_list) {
        auto diff = get_difference(calendar->validity_pattern.days, validity_pattern.days);
        size_t nb_diff = diff.count();

        LOG4CPLUS_TRACE(log, "cal " << calendar->uri << " :" << calendar->validity_pattern.days.to_string());

        //we associate the calendar to the vj if the diff are below a relative threshold
        //compared to the number of active days in the calendar
        size_t threshold = std::round(relative_threshold * calendar->validity_pattern.days.count());
        LOG4CPLUS_TRACE(log, "**** diff: " << nb_diff << " and threshold: " << threshold << (nb_diff <= threshold ? ", we keep it!!":""));

        if (nb_diff > threshold) {
            continue;
        }
        res.push_back({calendar, diff});
    }

    return res;
}

void Data::complete(){
    auto logger = log4cplus::Logger::getInstance("log");
    pt::ptime start;
    int sort, autocomplete;

    build_grid_validity_pattern();
    build_associated_calendar();
    build_odt();

    start = pt::microsec_clock::local_time();
    pt_data->sort();
    sort = (pt::microsec_clock::local_time() - start).total_milliseconds();

    start = pt::microsec_clock::local_time();
    LOG4CPLUS_INFO(logger, "Building proximity list");
    build_proximity_list();
    LOG4CPLUS_INFO(logger, "Building administrative regions");
    build_administrative_regions();
    LOG4CPLUS_INFO(logger, "Building uri maps");
    build_uri();
    LOG4CPLUS_INFO(logger, "Building autocomplete");
    build_autocomplete();

    /* ça devrait etre fait avant, à vérifier
    LOG4CPLUS_INFO(logger, "On va construire les correspondances");
    {Timer t("Construction des correspondances");  data.pt_data.build_connections();}
    */
    autocomplete = (pt::microsec_clock::local_time() - start).total_milliseconds();
    LOG4CPLUS_INFO(logger, "\t Sorting data: " << sort << "ms");
    LOG4CPLUS_INFO(logger, "\t Building autocomplete " << autocomplete << "ms");
}

ValidityPattern get_union_validity_pattern(const MetaVehicleJourney* meta_vj) {
    ValidityPattern validity;

    for (auto* vj: meta_vj->theoric_vj) {
        if (validity.beginning_date.is_not_a_date()) {
            validity.beginning_date = vj->validity_pattern->beginning_date;
        } else {
            if (validity.beginning_date != vj->validity_pattern->beginning_date) {
                throw navitia::exception("the beginning date of the meta_vj are not all the same");
            }
        }
        validity.days |= vj->validity_pattern->days;
    }
    return validity;
}

void Data::build_associated_calendar() {
    auto log = log4cplus::Logger::getInstance("kraken::type::Data");
    std::multimap<ValidityPattern, AssociatedCalendar*> associated_vp;
    size_t nb_not_matched_vj(0);
    size_t nb_matched(0);
    for(auto meta_vj_pair : this->pt_data->meta_vj) {
        auto meta_vj = meta_vj_pair.second;

        assert (! meta_vj->theoric_vj.empty());

        // we check the theoric vj of a meta vj
        // because we start from the postulate that the theoric VJs are the same VJ
        // split because of dst (day saving time)
        // because of that we try to match the calendar with the union of all theoric vj validity pattern
        ValidityPattern meta_vj_validity_pattern = get_union_validity_pattern(meta_vj);

        //some check can be done on any theoric vj, we do them on the first
        auto* first_vj = meta_vj->theoric_vj.front();
        const std::vector<Calendar*> calendar_list = first_vj->journey_pattern->route->line->calendar_list;
        if (calendar_list.empty()) {
            LOG4CPLUS_TRACE(log, "the line of the vj " << first_vj->uri << " is associated to no calendar");
            nb_not_matched_vj++;
            continue;
        }

        //we check if we already computed the associated val for this validity pattern
        //since a validity pattern can be shared by many vj
        auto it = associated_vp.find(meta_vj_validity_pattern);
        if (it != associated_vp.end()) {
            for (; it->first == meta_vj_validity_pattern; ++it) {
                meta_vj->associated_calendars.insert({it->second->calendar->uri, it->second});
            }
            continue;
        }

        auto close_cal = find_matching_calendar(*this, meta_vj_pair.first, meta_vj_validity_pattern, calendar_list);

        if (close_cal.empty()) {
            LOG4CPLUS_TRACE(log, "the meta vj " << meta_vj_pair.first << " has been attached to no calendar");
            nb_not_matched_vj++;
            continue;
        }
        nb_matched++;

        std::stringstream cal_uri;
        for (auto cal_bit_set: close_cal) {
            auto associated_calendar = new AssociatedCalendar();
            pt_data->associated_calendars.push_back(associated_calendar);

            associated_calendar->calendar = cal_bit_set.first;
            //we need to create the associated exceptions
            for (size_t i = 0; i < cal_bit_set.second.size(); ++i) {
                if (! cal_bit_set.second[i]) {
                    continue; //cal_bit_set.second is the resulting differences, so 0 means no exception
                }
                ExceptionDate ex;
                ex.date = meta_vj_validity_pattern.beginning_date + boost::gregorian::days(i);
                //if the vj is active this day it's an addition, else a removal
                ex.type = (meta_vj_validity_pattern.days[i] ? ExceptionDate::ExceptionType::add : ExceptionDate::ExceptionType::sub);
                associated_calendar->exceptions.push_back(ex);
            }

            meta_vj->associated_calendars.insert({associated_calendar->calendar->uri, associated_calendar});
            associated_vp.insert({meta_vj_validity_pattern, associated_calendar});
            cal_uri << associated_calendar->calendar->uri << " ";
        }

        LOG4CPLUS_DEBUG(log, "the meta vj " << meta_vj_pair.first << " has been attached to " << cal_uri.str());
    }

    LOG4CPLUS_INFO(log, nb_matched << " vehicle journeys have been matched to at least one calendar");
    if (nb_not_matched_vj) {
        LOG4CPLUS_WARN(log, "no calendar found for " << nb_not_matched_vj << " vehicle journey");
    }
}

void Data::build_odt(){
    for(JourneyPattern* jp : this->pt_data->journey_patterns){
        jp->odt_level = type::OdtLevel_e::none;
        VehicleJourney* vj;
        if(jp->vehicle_journey_list.empty()){
            continue;
        }
        vj = jp->vehicle_journey_list.front();
        jp->odt_level = vj->get_odt_level();
        for(idx_t idx = 1; idx < jp->vehicle_journey_list.size(); idx++){
            vj = jp->vehicle_journey_list[idx];
            if (vj->get_odt_level() != jp->odt_level){
                jp->odt_level = type::OdtLevel_e::mixt;
                break;
            }
        }
    }
}

void Data::build_grid_validity_pattern() {
    for(Calendar* cal : this->pt_data->calendars){
        cal->build_validity_pattern(meta->production_date);
    }
}

#define GET_DATA(type_name, collection_name)\
template<> std::vector<type_name*> & \
Data::get_data<type_name>() {\
    return this->pt_data->collection_name;\
}\
template<> std::vector<type_name *> \
Data::get_data<type_name>() const {\
    return this->pt_data->collection_name;\
}
ITERATE_NAVITIA_PT_TYPES(GET_DATA)

template<> std::vector<georef::POI*> &
Data::get_data<georef::POI>() {
    return this->geo_ref->pois;
}
template<> std::vector<georef::POI*>
Data::get_data<georef::POI>() const {
    return this->geo_ref->pois;
}

template<> std::vector<georef::POIType*> &
Data::get_data<georef::POIType>() {
    return this->geo_ref->poitypes;
}
template<> std::vector<georef::POIType*>
Data::get_data<georef::POIType>() const {
    return this->geo_ref->poitypes;
}

template<> std::vector<StopPointConnection*> &
Data::get_data<StopPointConnection>() {
    return this->pt_data->stop_point_connections;
}
template<> std::vector<StopPointConnection*>
Data::get_data<StopPointConnection>() const {
    return this->pt_data->stop_point_connections;
}


std::vector<idx_t> Data::get_all_index(Type_e type) const {
    size_t num_elements = 0;
    switch(type){
    #define GET_NUM_ELEMENTS(type_name, collection_name)\
    case Type_e::type_name:\
        num_elements = this->pt_data->collection_name.size();break;
    ITERATE_NAVITIA_PT_TYPES(GET_NUM_ELEMENTS)
    case Type_e::POI: num_elements = this->geo_ref->pois.size(); break;
    case Type_e::POIType: num_elements = this->geo_ref->poitypes.size(); break;
    case Type_e::Connection:
        num_elements = this->pt_data->stop_point_connections.size(); break;
    default:  break;
    }
    std::vector<idx_t> indexes(num_elements);
    for(size_t i=0; i < num_elements; i++)
        indexes[i] = i;
    return indexes;
}



std::vector<idx_t>
Data::get_target_by_source(Type_e source, Type_e target,
                           std::vector<idx_t> source_idx) const {
    std::vector<idx_t> result;
    result.reserve(source_idx.size());
    for(idx_t idx : source_idx) {
        std::vector<idx_t> tmp;
        tmp = get_target_by_one_source(source, target, idx);
        result.insert(result.end(), tmp.begin(), tmp.end());
    }
    return result;
}

std::vector<idx_t>
Data::get_target_by_one_source(Type_e source, Type_e target,
                               idx_t source_idx) const {
    std::vector<idx_t> result;
    if(source_idx == invalid_idx)
        return result;
    if(source == target){
        result.push_back(source_idx);
        return result;
    }
    switch(source) {
    #define GET_INDEXES(type_name, collection_name)\
        case Type_e::type_name:\
            result = pt_data->collection_name[source_idx]->get(target, *pt_data);\
            break;
    ITERATE_NAVITIA_PT_TYPES(GET_INDEXES)
        case Type_e::POI:
            result = geo_ref->pois[source_idx]->get(target, *geo_ref);
        case Type_e::POIType:
            result = geo_ref->poitypes[source_idx]->get(target, *geo_ref);
        default: break;
    }
    return result;
}

Type_e Data::get_type_of_id(const std::string & id) const {
    if(id.size()>6 && id.substr(0,6) == "coord:")
        return Type_e::Coord;
    if(id.size()>6 && id.substr(0,8) == "address:")
        return Type_e::Address;
    if(id.size()>6 && id.substr(0,6) == "admin:")
        return Type_e::Admin;
    #define GET_TYPE(type_name, collection_name) \
    auto collection_name##_map = pt_data->collection_name##_map;\
    if(collection_name##_map.find(id) != collection_name##_map.end())\
        return Type_e::type_name;
    ITERATE_NAVITIA_PT_TYPES(GET_TYPE)
    if(geo_ref->poitype_map.find(id) != geo_ref->poitype_map.end())
        return Type_e::POIType;
    if(geo_ref->poi_map.find(id) != geo_ref->poi_map.end())
        return Type_e::POI;
    if(geo_ref->way_map.find(id) != geo_ref->way_map.end())
        return Type_e::Address;
    if(geo_ref->admin_map.find(id) != geo_ref->admin_map.end())
        return Type_e::Admin;
    return Type_e::Unknown;
}


}} //namespace navitia::type
