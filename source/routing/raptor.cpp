#include "raptor.h"
#include <boost/foreach.hpp>

namespace bt = boost::posix_time;

namespace navitia { namespace routing {

void RAPTOR::make_queue() {
    marked_rp.reset();
    marked_sp.reset();
}

template<class Visitor>
void
RAPTOR::journey_pattern_path_connections(const Visitor & visitor/*,
                                         const std::bitset<7> & required_properties*/) {
    std::vector<type::idx_t> to_mark;
    for(auto jpp_departure_idx = marked_rp.find_first(); jpp_departure_idx != marked_rp.npos; jpp_departure_idx = marked_rp.find_next(jpp_departure_idx)) {
        const auto* jpp_departure = data.pt_data.journey_pattern_points[jpp_departure_idx];
        BOOST_FOREACH(auto &idx_rpc, data.dataRaptor.footpath_rp(visitor.clockwise()).equal_range(jpp_departure_idx)) {
            const auto & rpc = idx_rpc.second;
            const type::JourneyPatternPoint* jpp = rpc->*visitor.journey_pattern_point();

            type::idx_t jpp_idx = jpp->idx;
            DateTime dt = visitor.combine(labels[count][jpp_departure_idx].dt, rpc->duration);
            if(get_type(count, jpp_departure_idx) == boarding_type::vj && visitor.comp(dt, best_labels[jpp_idx])) {
                labels[count][jpp_idx].dt = dt;
                best_labels[jpp_idx] = dt;
                labels[count][jpp_idx].boarding = jpp_departure;
                labels[count][jpp_idx].type = boarding_type::connection_stay_in;
                to_mark.push_back(jpp_idx);
            }
        }
    }

    for(auto rp : to_mark) {
        marked_rp.set(rp);
        const auto* journey_pattern_point = data.pt_data.journey_pattern_points[rp];
        if(visitor.comp(journey_pattern_point->order, Q[journey_pattern_point->journey_pattern->idx]) ) {
            Q[journey_pattern_point->journey_pattern->idx] = journey_pattern_point->order;
        }
    }
}


template<typename Visitor>
void RAPTOR::foot_path(const Visitor & v, const type::Properties &required_properties) {

    int last = 0;
    const auto foot_path_list = v.clockwise() ? data.dataRaptor.foot_path_forward :
                                                data.dataRaptor.foot_path_backward;
    auto it = foot_path_list.begin();
    auto &current_labels = labels[count];
    for(auto stop_point_idx = marked_sp.find_first(); stop_point_idx != marked_sp.npos;
        stop_point_idx = marked_sp.find_next(stop_point_idx)) {
        //On cherche le meilleur jpp du stop point
        const type::StopPoint* stop_point = data.pt_data.stop_points[stop_point_idx];
        if(stop_point->accessible(required_properties)) {
            DateTime best_arrival = v.worst_datetime();
            type::idx_t best_jpp = type::invalid_idx;

            for(auto journey_pattern_point : stop_point->journey_pattern_point_list) {
                type::idx_t jppidx = journey_pattern_point->idx;
                boarding_type b_type = get_type(count, jppidx);
                //On regarde si on est arrivé avec un vj ou un departure,
                //Puis on compare avec la meilleure arrivée trouvée pour ce stoppoint
                if((b_type == boarding_type::vj || b_type == boarding_type::departure) &&
                    v.comp(current_labels[jppidx].dt, best_arrival)) {
                    best_arrival = current_labels[jppidx].dt;
                    best_jpp = jppidx;
                }
            }
            // Si on a trouvé un journey pattern pour ce stop point
            // NB : l'inverse arrive lorsqu'on a déjà marqué le stop point avec une autre correspondance
            if(best_jpp != type::invalid_idx) {
                const DateTime best_departure = v.combine(best_arrival, 120);
                //On marque tous les journey_pattern points du stop point
                for(auto jpp : stop_point->journey_pattern_point_list) {
                    type::idx_t jpp_idx = jpp->idx;
                    if(jpp_idx != best_jpp && v.comp(best_departure, best_labels[jpp_idx]) && !b_dest.is_eligible_solution(jpp_idx)) {
                       current_labels[jpp_idx].dt = best_departure;
                       current_labels[jpp_idx].boarding = data.pt_data.journey_pattern_points[best_jpp];
                       current_labels[jpp_idx].type = boarding_type::connection;
                       best_labels[jpp_idx] = best_departure;

                       if(v.comp(jpp->order, Q[jpp->journey_pattern->idx])) {
                           Q[jpp->journey_pattern->idx] = jpp->order;
                       }
                    }
                }
                //On va maintenant chercher toutes les connexions et on marque tous les journey_pattern_points concernés
                //On récupère l'index dans les footpath
                const pair_int & index = (v.clockwise()) ? data.dataRaptor.footpath_index_forward[stop_point_idx] :
                                                         data.dataRaptor.footpath_index_backward[stop_point_idx];
                //int prec_duration = -1;
                DateTime next = v.worst_datetime(),
                         previous = current_labels[best_jpp].dt;
                it += index.first - last;
                const auto end = it + index.second;

                for(; it != end; ++it) {
                    const type::StopPointConnection* spc = *it;
                    const auto destination = spc->destination;
                    next = v.combine(previous, spc->duration); // ludo
                    if(destination->accessible(required_properties)) {
                        for(auto destination_jpp : destination->journey_pattern_point_list) {
                            type::idx_t destination_jpp_idx = destination_jpp->idx;
                            if(best_jpp != destination_jpp_idx && !b_dest.is_eligible_solution(destination_jpp_idx)) {
                                if(v.comp(next, best_labels[destination_jpp_idx]) || next == best_labels[destination_jpp_idx]) {
                                    current_labels[destination_jpp_idx].dt = next;
                                    current_labels[destination_jpp_idx].boarding = data.pt_data.journey_pattern_points[best_jpp];
                                    current_labels[destination_jpp_idx].type = boarding_type::connection;
                                    best_labels[destination_jpp_idx] = next;

                                    if(v.comp(destination_jpp->order, Q[destination_jpp->journey_pattern->idx])) {
                                        Q[destination_jpp->journey_pattern->idx] = destination_jpp->order;
                                   }
                                }
                            }
                        }
                    }
                }
                last = index.first + index.second;
            }
        }
    }
}

void RAPTOR::clear(const type::Data & data, bool clockwise, DateTime borne) {
    if(clockwise) {
        //Q.assign(data.pt_data.journey_patterns.size(), std::numeric_limits<int>::max());
        memset32<int>(&Q[0], data.pt_data.journey_patterns.size(), std::numeric_limits<int>::max());
        labels.resize(1);
        labels[0] = data.dataRaptor.labels_const;
    } else {
        //Q.assign(data.pt_data.journey_patterns.size(), -1);
        memset32<int>(&Q[0], data.pt_data.journey_patterns.size(), -1);
        labels.resize(1);
        labels[0] = data.dataRaptor.labels_const_reverse;
    }

    b_dest.reinit(data.pt_data.journey_pattern_points.size(), borne);
    this->make_queue();
    if(clockwise)
        best_labels.assign(data.pt_data.journey_pattern_points.size(), DateTimeUtils::inf);
    else
        best_labels.assign(data.pt_data.journey_pattern_points.size(), DateTimeUtils::min);
}

void RAPTOR::clear_and_init(std::vector<Departure_Type> departures,
                  std::vector<std::pair<type::EntryPoint, std::vector<std::pair<type::idx_t, bt::time_duration> > > > multi_arrivals,
                  DateTime bound,  const bool clockwise,
                  const type::Properties &required_properties) {

    this->clear(data, clockwise, bound);

    for(Departure_Type item : departures) {
        const type::JourneyPatternPoint* journey_pattern_point = data.pt_data.journey_pattern_points[item.rpidx];
        const type::StopPoint* stop_point = journey_pattern_point->stop_point;
        if(stop_point->accessible(required_properties)) {
            labels[0][item.rpidx].dt = item.arrival;
            labels[0][item.rpidx].type = boarding_type::departure;
            best_labels[item.rpidx] = item.arrival;

            if(clockwise && Q[journey_pattern_point->journey_pattern->idx] > journey_pattern_point->order)
                Q[journey_pattern_point->journey_pattern->idx] = journey_pattern_point->order;
            else if(!clockwise &&  Q[journey_pattern_point->journey_pattern->idx] < journey_pattern_point->order)
                Q[journey_pattern_point->journey_pattern->idx] = journey_pattern_point->order;
            if(item.arrival != DateTimeUtils::min && item.arrival != DateTimeUtils::inf) {
                marked_sp.set(stop_point->idx);
            }
        }
    }

    for(int i = 0; i < multi_arrivals.size(); i++) {
        auto arrival_pair = multi_arrivals[i];
        type::EntryPoint arrival_entrypoint = arrival_pair.first;
        int access_duration = arrival_entrypoint.access_duration;
        auto destinations = arrival_pair.second;

        for(auto item : destinations) {
            const type::StopPoint* sp = data.pt_data.stop_points[item.first];
            if(sp->accessible(required_properties)) {
                for(auto journey_pattern_point : sp->journey_pattern_point_list) {
                    type::idx_t jppidx = journey_pattern_point->idx;
                    if(journey_patterns_valides.test(journey_pattern_point->journey_pattern->idx) &&
                            ((clockwise && (bound == DateTimeUtils::inf || best_labels[jppidx] > bound)) ||
                            ((!clockwise) && (bound == DateTimeUtils::min || best_labels[jppidx] < bound)))) {
                            b_dest.add_destination(jppidx, item.second, clockwise);
                            best_labels[jppidx] = bound;
                        }
                }
            }
        }
    }
}


std::vector<Path>
RAPTOR::compute_all(const std::vector<std::pair<type::EntryPoint, std::vector<std::pair<type::idx_t, bt::time_duration> > > > &multi_calc_dep,
                    const std::vector<std::pair<type::EntryPoint, std::vector<std::pair<type::idx_t, bt::time_duration> > > > &multi_calc_arr,
                    const DateTime &departure_datetime,
                    bool disruption_active, const DateTime &bound,
                    const uint32_t max_transfers,
                    const type::AccessibiliteParams & accessibilite_params,
                    const std::vector<std::string> & forbidden,
                    bool clockwise, bool details) {
    std::vector<Path> result;
    set_journey_patterns_valides(DateTimeUtils::date(departure_datetime), forbidden, disruption_active);

    std::vector<Departure_Type> departures;
    for (auto calc_dep_pair : multi_calc_dep) {
        type::EntryPoint calc_dep_entrypoint = calc_dep_pair.first;
        auto calc_dep = calc_dep_pair.second;
        auto local_departures = getDepartures(calc_dep, departure_datetime + calc_dep_entrypoint.access_duration, clockwise, data, disruption_active);
        departures.insert(departures.end(), local_departures.begin(), local_departures.end());
    }

    clear_and_init(departures, multi_calc_arr, bound, clockwise);

    boucleRAPTOR(accessibilite_params, clockwise, disruption_active, false, max_transfers);
    //auto tmp = makePathes(calc_dest, bound, accessibilite_params, *this, clockwise, disruption_active);
    //result.insert(result.end(), tmp.begin(), tmp.end());

    // No solution found
    if(b_dest.best_now_jpp_idx == type::invalid_idx)
        return result;

    // No details?
    if (!details) {
        for (auto calc_arr_item : multi_calc_arr) {
            type::EntryPoint calc_arr_entrypoint = calc_arr_item.first;
            auto calc_arr = calc_arr_item.second;
            std::vector<Path> temp = makePathes(calc_arr, departure_datetime, accessibilite_params/*required_properties*/, *this, clockwise, disruption_active);

            // Add missing origin & destination data
            BOOST_FOREACH(Path& path, temp)
            {
                path.origin = multi_calc_dep[0].first;
                path.destination = calc_arr_entrypoint;
            }

            result.insert(result.end(), temp.begin(), temp.end());
        }
        return result;
    }

    //Second pass : interchange optimization
    auto calc_dep = multi_calc_dep[0].second;
    auto calc_arr = multi_calc_arr[0].second;

    departures = getDepartures(calc_dep, calc_arr, !clockwise,
                               labels,
                               accessibilite_params, data, disruption_active);

    for(auto departure : departures) {
        clear_and_init({departure}, {multi_calc_dep[0]}, departure_datetime, !clockwise);

        boucleRAPTOR(accessibilite_params, !clockwise, disruption_active, true, max_transfers);

        if(b_dest.best_now_jpp_idx != type::invalid_idx) {
            std::vector<Path> temp = makePathes(calc_dep, departure_datetime, accessibilite_params, *this, !clockwise, disruption_active);
            result.insert(result.end(), temp.begin(), temp.end());
        }
    }
    return result;
}


void
RAPTOR::isochrone(const std::vector<std::pair<type::idx_t, bt::time_duration> > &departures_,
          const DateTime &departure_datetime, const DateTime &bound, uint32_t max_transfers,
          const type::AccessibiliteParams & accessibilite_params,
          const std::vector<std::string> & forbidden,
          bool clockwise, bool disruption_active) {
    set_journey_patterns_valides(DateTimeUtils::date(departure_datetime), forbidden, disruption_active);
    auto departures = getDepartures(departures_, departure_datetime, true, data, disruption_active);
    clear_and_init(departures, {}, bound, true);

    boucleRAPTOR(accessibilite_params, clockwise, true, max_transfers);
}


void RAPTOR::set_journey_patterns_valides(uint32_t date, const std::vector<std::string> & forbidden, bool disruption_active) {

    if(disruption_active){
        journey_patterns_valides = data.dataRaptor.jp_adapted_validity_pattern[date];
    }else{
        journey_patterns_valides = data.dataRaptor.jp_validity_patterns[date];
    }
    boost::dynamic_bitset<> forbidden_journey_patterns(data.pt_data.journey_patterns.size());
    for(const type::JourneyPattern* journey_pattern : data.pt_data.journey_patterns) {
        const type::Line* line = journey_pattern->route->line;
        // On gère la liste des interdits
        for(auto forbid_uri : forbidden){
            if       ( (forbid_uri == line->uri)
                    || (forbid_uri == journey_pattern->route->uri)
                    || (forbid_uri == journey_pattern->uri)
                    || (forbid_uri == journey_pattern->commercial_mode->uri)
                    || (forbid_uri == journey_pattern->physical_mode->uri)
                    || (forbid_uri == line->network->uri) )
            {
                forbidden_journey_patterns.set(journey_pattern->idx);
                break;
            }
        }
    }
    journey_patterns_valides &= ~forbidden_journey_patterns;
}


struct raptor_visitor {
    inline bool better_or_equal(const DateTime &a, const DateTime &current_dt, const type::StopTime* st) const {
        return a <= st->section_end_date(DateTimeUtils::date(current_dt), clockwise());
    }

    inline
    std::pair<std::vector<type::JourneyPatternPoint*>::const_iterator, std::vector<type::JourneyPatternPoint*>::const_iterator>
    journey_pattern_points(const std::vector<type::JourneyPatternPoint*> &, const type::JourneyPattern* journey_pattern, size_t order) const {
        return std::make_pair(journey_pattern->journey_pattern_point_list.begin() + order,
                              journey_pattern->journey_pattern_point_list.end());
    }

    typedef std::vector<type::StopTime*>::const_iterator stop_time_iterator;
    inline stop_time_iterator first_stoptime(const type::StopTime* st) const {
        const type::JourneyPatternPoint* jpp = st->journey_pattern_point;
        const type::VehicleJourney* vj = st->vehicle_journey;
        return vj->stop_time_list.begin() + jpp->order;
    }

    template<typename T1, typename T2> inline bool comp(const T1& a, const T2& b) const {
        return a < b;
    }

    template<typename T1, typename T2> inline auto combine(const T1& a, const T2& b) const -> decltype(a+b) {
        return a + b;
    }

    constexpr bool clockwise() const{return true;}
    constexpr int init_queue_item() const{return std::numeric_limits<int>::max();}
    constexpr DateTime worst_datetime() const{return DateTimeUtils::inf;}
    constexpr type::JourneyPatternPoint* type::JourneyPatternPointConnection::* journey_pattern_point() const{return &type::JourneyPatternPointConnection::destination;}
};


struct raptor_reverse_visitor {
    inline bool better_or_equal(const DateTime &a, const DateTime &current_dt, const type::StopTime* st) const {
        return a >= st->section_end_date(DateTimeUtils::date(current_dt), clockwise());
    }

    inline
    std::pair<std::vector<type::JourneyPatternPoint*>::const_reverse_iterator, std::vector<type::JourneyPatternPoint*>::const_reverse_iterator>
    journey_pattern_points(const std::vector<type::JourneyPatternPoint*> &/*journey_pattern_points*/, const type::JourneyPattern* journey_pattern, size_t order) const {
        size_t offset = journey_pattern->journey_pattern_point_list.size() - order - 1;
        const auto begin = journey_pattern->journey_pattern_point_list.rbegin() + offset;
        const auto end = journey_pattern->journey_pattern_point_list.rend();
        return std::make_pair(begin, end);
    }

    typedef std::vector<type::StopTime*>::const_reverse_iterator stop_time_iterator;
    inline stop_time_iterator first_stoptime(const type::StopTime* st) const {
        const type::JourneyPatternPoint* jpp = st->journey_pattern_point;
        const type::VehicleJourney* vj = st->vehicle_journey;
        return vj->stop_time_list.rbegin() + vj->stop_time_list.size() - jpp->order - 1;
    }

    template<typename T1, typename T2> inline bool comp(const T1& a, const T2& b) const {
        return a > b;
    }

    template<typename T1, typename T2> inline auto combine(const T1& a, const T2& b) const -> decltype(a-b) {
        return a - b;
    }

    constexpr bool clockwise() const{return false;}
    constexpr int init_queue_item() const{return -1;}
    constexpr DateTime worst_datetime() const{return DateTimeUtils::min;}
    constexpr type::JourneyPatternPoint* type::JourneyPatternPointConnection::* journey_pattern_point() const{return &type::JourneyPatternPointConnection::departure;}
};


template<typename Visitor>
void RAPTOR::raptor_loop(Visitor visitor, const type::AccessibiliteParams & accessibilite_params, bool disruption_active,
        bool global_pruning, uint32_t max_transfers) {
    bool end = false;
    count = 0; //< Itération de l'algo raptor (une itération par correspondance)
    const type::JourneyPatternPoint* boarding = nullptr; //< Le JPP time auquel on a embarqué
    DateTime workingDt = visitor.worst_datetime();
    uint32_t l_zone = std::numeric_limits<uint32_t>::max();

    //this->foot_path(visitor, accessibilite_params.properties);
    uint32_t nb_jpp_visites = 0;
    while(!end && count <= max_transfers) {
        ++count;
        end = true;
        if(count == labels.size()) {
            if(visitor.clockwise()) {
                this->labels.push_back(this->data.dataRaptor.labels_const);
            } else {
                this->labels.push_back(this->data.dataRaptor.labels_const_reverse);
            }
       	}
        const auto & prec_labels=labels[count -1];
        auto & working_labels = labels[this->count];
        this->make_queue();

        for(const auto & journey_pattern : data.pt_data.journey_patterns) {
            if(Q[journey_pattern->idx] != std::numeric_limits<int>::max()
                    && Q[journey_pattern->idx] != -1
                    && journey_patterns_valides.test(journey_pattern->idx)) {
                nb_jpp_visites ++;
                boarding = nullptr;
                workingDt = visitor.worst_datetime();
                typename Visitor::stop_time_iterator it_st;
                const auto & jpp_to_explore = visitor.journey_pattern_points(
                                                this->data.pt_data.journey_pattern_points,
                                                journey_pattern,Q[journey_pattern->idx]);
                BOOST_FOREACH(const type::JourneyPatternPoint* jpp, jpp_to_explore) {
                    if(!jpp->stop_point->accessible(accessibilite_params.properties)) {
                        continue;
                    }
                    type::idx_t jpp_idx = jpp->idx;
                    if(boarding != nullptr) {
                        ++it_st;
                        const type::StopTime* st = *it_st;
                        const auto current_time = st->section_end_time(visitor.clockwise(), DateTimeUtils::hour(workingDt));
                        DateTimeUtils::update(workingDt, current_time, visitor.clockwise());
                        if((l_zone == std::numeric_limits<uint32_t>::max()
                            || l_zone != st->local_traffic_zone)
                                && st->valid_end(visitor.clockwise())) {
                            //On stocke le meilleur label, et on marque pour explorer par la suite
                            const DateTime bound = (visitor.comp(best_labels[jpp_idx], b_dest.best_now) || !global_pruning) ?
                                                    best_labels[jpp_idx] : b_dest.best_now;

                            if(visitor.comp(workingDt, bound)) {
                                working_labels[jpp_idx].dt = workingDt;
                                working_labels[jpp_idx].boarding = boarding;
                                working_labels[jpp_idx].type = boarding_type::vj;
                                best_labels[jpp_idx] = working_labels[jpp_idx].dt;
                                if(!this->b_dest.add_best(visitor, jpp_idx, working_labels[jpp_idx].dt, this->count)) {
                                    this->marked_rp.set(jpp_idx);
                                    this->marked_sp.set(jpp->stop_point->idx);
                                    end = false;
                                }
                            } else if(workingDt == bound &&
                                      get_type(this->count-1, jpp_idx) == boarding_type::uninitialized &&
                                      b_dest.add_best(visitor, jpp_idx, workingDt, this->count)) {
                                working_labels[jpp_idx].dt = workingDt;
                                working_labels[jpp_idx].boarding = boarding;
                                working_labels[jpp_idx].type = boarding_type::vj;
                                best_labels[jpp_idx] = workingDt;
                            }
                        }
                    }

                    //Si on peut arriver plus tôt à l'arrêt en passant par une autre journey_pattern
                    const DateTime labels_temp = prec_labels[jpp_idx].dt;
                    const boarding_type b_type = get_type(this->count-1, jpp_idx);
                    if(b_type != boarding_type::uninitialized && b_type != boarding_type::vj &&
                       (boarding == nullptr || visitor.better_or_equal(labels_temp, workingDt, *it_st))) {
                        const auto tmp_st_dt = best_stop_time(jpp, labels_temp,
                                                                accessibilite_params.vehicle_properties,
                                                                visitor.clockwise(), disruption_active, data);
                        if(tmp_st_dt.first != nullptr) {
                            boarding = jpp;
                            it_st = visitor.first_stoptime(tmp_st_dt.first);
                            workingDt = tmp_st_dt.second;
                            BOOST_ASSERT(visitor.comp(labels_temp, workingDt) || labels_temp == workingDt);
                            l_zone = (*it_st)->local_traffic_zone;
                        }
                    }
                }
            }
            Q[journey_pattern->idx] = visitor.init_queue_item();
        }
        // Prolongements de service
        this->journey_pattern_path_connections(visitor);
        // Correspondances
        this->foot_path(visitor, accessibilite_params.properties);
    }
}


void RAPTOR::boucleRAPTOR(const type::AccessibiliteParams & accessibilite_params, bool clockwise, bool disruption_active, bool global_pruning, uint32_t max_transfers){
    if(clockwise) {
        raptor_loop(raptor_visitor(), accessibilite_params, disruption_active, global_pruning, max_transfers);
    } else {
        raptor_loop(raptor_reverse_visitor(), accessibilite_params, disruption_active, global_pruning, max_transfers);
    }
}


std::vector<Path> RAPTOR::compute(const type::StopArea* departure,
        const type::StopArea* destination, int departure_hour,
        int departure_day, DateTime borne, bool disruption_active, bool clockwise,
        const type::AccessibiliteParams & accessibilite_params,
        uint32_t max_transfers) {
    std::vector<std::pair<type::idx_t, bt::time_duration> > departures, destinations;

    for(const type::StopPoint* sp : departure->stop_point_list) {
        departures.push_back({sp->idx, {}});
    }

    for(const type::StopPoint* sp : destination->stop_point_list) {
        destinations.push_back({sp->idx, {}});
    }

    auto calc_dep = {std::make_pair(type::EntryPoint(), clockwise ? departures : destinations)};
    auto calc_dest = {std::make_pair(type::EntryPoint(), clockwise ? destinations : departures)};

    return compute_all(calc_dep, calc_dest, DateTimeUtils::set(departure_day, departure_hour), disruption_active, borne, max_transfers, accessibilite_params, {}, clockwise);
}


int RAPTOR::best_round(type::idx_t journey_pattern_point_idx){
    for(size_t i = 0; i < labels.size(); ++i){
        if(labels[i][journey_pattern_point_idx].dt == best_labels[journey_pattern_point_idx]){
            return i;
        }
    }
    return -1;
}

}}
