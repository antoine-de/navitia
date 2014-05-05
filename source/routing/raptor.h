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

#pragma once
#include <unordered_map>
#include <limits>
#include "type/type.h"
#include "type/data.h"
#include "type/datetime.h"
#include "routing.h"
#include "utils/timer.h"
#include "boost/dynamic_bitset.hpp"
#include "dataraptor.h"
#include "best_stoptime.h"
#include "raptor_path.h"
#include "raptor_solutions.h"
#include "raptor_utils.h"

namespace navitia { namespace routing {

/** Worker Raptor : une instance par thread, les données sont modifiées par le calcul */
struct RAPTOR
{
    const navitia::type::Data & data;

    ///Contient les heures d'arrivées, de départ, ainsi que la façon dont on est arrivé à chaque journey_pattern point à chaque tour
    std::vector<label_vector_t> labels;

    ///Contient les meilleures heures d'arrivées, de départ, ainsi que la façon dont on est arrivé à chaque journey_pattern point
    std::vector<DateTime> best_labels;//REVIEW: du coup c'est pas un vecteur de label, c'est zarb pour un truc qui s'appelle best_labels :)
    ///Contient tous les points d'arrivée, et la meilleure façon dont on est arrivé à destination
    best_dest b_dest;
    ///Nombre de correspondances effectuées jusqu'à présent
    unsigned int count;
    ///Est-ce que le journey_pattern point est marqué ou non ?
    boost::dynamic_bitset<> marked_rp;
    ///Est-ce que le stop point est arrivé ou non ?
    boost::dynamic_bitset<> marked_sp;
    ///La journey_pattern est elle valide ?
    boost::dynamic_bitset<> journey_patterns_valides;
    ///L'ordre du premier j: public AbstractRouterourney_pattern point de la journey_pattern
    queue_t Q;//REVIEW: traduire les commentaires, et les changer par la meme occaz' je suis pas certain que ca soit encore d'actualite :p
    //REVIEW: pour les vector je trouve ca pas mal d'indique par quoi c'est indexe, vu que c'est pas dans le type

    //Constructeur
    RAPTOR(const navitia::type::Data &data) :
        data(data), best_labels(data.pt_data->journey_pattern_points.size()), count(0),
        marked_rp(data.pt_data->journey_pattern_points.size()),
        marked_sp(data.pt_data->stop_points.size()),
        journey_patterns_valides(data.pt_data->journey_patterns.size()),
        Q(data.pt_data->journey_patterns.size()) {
            labels.assign(20, data.dataRaptor->labels_const);
    }



    void clear(const type::Data & data, bool clockwise, DateTime borne);

    ///Initialise les structure retour et b_dest
    void clear_and_init(Solutions departures,
              std::vector<std::pair<type::idx_t, boost::posix_time::time_duration> > destinations,
              navitia::DateTime bound, const bool clockwise,
              const type::Properties &properties = 0);


    ///Lance un calcul d'itinéraire entre deux stop areas avec aussi une borne
    std::vector<Path>
    compute(const type::StopArea* departure, const type::StopArea* destination,
            int departure_hour, int departure_day, DateTime bound, bool disruption_active,
            bool clockwise = true,
            /*const type::Properties &required_properties = 0*/
            const type::AccessibiliteParams & accessibilite_params = type::AccessibiliteParams(), uint32_t
            max_transfers=std::numeric_limits<uint32_t>::max());


    /** Calcul d'itinéraires dans le sens horaire à partir de plusieurs 
     *  stop points de départs, vers plusieurs stoppoints d'arrivée,
     *  à une heure donnée.
     */
    std::vector<Path> 
    compute_all(const std::vector<std::pair<type::idx_t, boost::posix_time::time_duration>> &departs,
                const std::vector<std::pair<type::idx_t, boost::posix_time::time_duration>> &destinations,
                const DateTime &departure_datetime, bool disruption_active, const DateTime &bound=DateTimeUtils::inf,
                const uint32_t max_transfers=std::numeric_limits<int>::max(),
                const type::AccessibiliteParams & accessibilite_params = type::AccessibiliteParams(),
                const std::vector<std::string> & forbidden = std::vector<std::string>(), bool clockwise=true);


    
    /** Calcul l'isochrone à partir de tous les points contenus dans departs,
     *  vers tous les autres points.
     *  Renvoie toutes les arrivées vers tous les stop points.
     */
    void
    isochrone(const std::vector<std::pair<type::idx_t, boost::posix_time::time_duration>> &departures_,
              const DateTime &departure_datetime, const DateTime &bound = DateTimeUtils::min,
              uint32_t max_transfers = std::numeric_limits<uint32_t>::max(),
              const type::AccessibiliteParams & accessibilite_params = type::AccessibiliteParams(),
              const std::vector<std::string>& forbidden = std::vector<std::string>(),
              bool clockwise = true, bool disruption_active = false);


    /// Désactive les journey_patterns qui n'ont pas de vj valides la veille, le jour, et le lendemain du calcul
    /// Gère également les lignes, modes, journey_patterns et VJ interdits
    void set_journey_patterns_valides(uint32_t date, const std::vector<std::string> & forbidden, bool disruption_active);

    ///Boucle principale, parcourt les journey_patterns,
    void boucleRAPTOR(const type::AccessibiliteParams & accessibilite_params, bool clockwise, bool disruption_active,
                      bool global_pruning = true,
                      const uint32_t max_transfers=std::numeric_limits<uint32_t>::max());

    /// Fonction générique pour la marche à pied
    /// Il faut spécifier le visiteur selon le sens souhaité
    template<typename Visitor> void foot_path(const Visitor & v, const type::Properties &required_properties);

    ///Correspondances garanties et prolongements de service
    template<typename Visitor> void journey_pattern_path_connections(const Visitor &visitor/*, const type::Properties &required_properties*/);

    ///Trouve pour chaque journey_pattern, le premier journey_pattern point auquel on peut embarquer, se sert de marked_rp
    void make_queue();

    ///Boucle principale
    template<typename Visitor>
    void raptor_loop(Visitor visitor, const type::AccessibiliteParams & accessibilite_params, bool disruption_active, bool global_pruning = true, uint32_t max_transfers=std::numeric_limits<uint32_t>::max());


    /// Retourne à quel tour on a trouvé la meilleure solution pour ce journey_patternpoint
    /// Retourne -1 s'il n'existe pas de meilleure solution
    int best_round(type::idx_t journey_pattern_point_idx);

    inline boarding_type get_type(size_t count, type::idx_t jpp_idx) const {
        return labels[count][jpp_idx].type;
    }

    inline const type::JourneyPatternPoint* get_boarding_jpp(size_t count, type::idx_t jpp_idx) const {
        return labels[count][jpp_idx].boarding;
    }

    ~RAPTOR() {}
};

}}
