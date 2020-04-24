#include <iostream>
#include <random>

#include "data_structures.hpp"
#include "dispatch.hpp"
#include "optimizer.hpp"

typedef std::mt19937 our_random_engine;

void seed_rand(our_random_engine &eng, const uint32_t seed){
  if(seed==0){
    std::uint_least32_t seed_data[std::mt19937::state_size];
    std::random_device r;
    std::generate_n(seed_data, std::mt19937::state_size, std::ref(r));
    std::seed_seq q(std::begin(seed_data), std::end(seed_data));
    eng.seed(q);
  } else {
    eng.seed(seed);
  }
}

double r_uniform(our_random_engine &eng, double from, double thru){
  std::uniform_real_distribution<double> distribution(from,thru);
  return distribution(eng);
}

typedef std::vector<ModelResults> results_vec;

void spawn_from(
  const ModelResults parent, //Don't reference, since we're resizing vector
  const int count,
  const double mutation_rate,
  our_random_engine &eng,
  results_vec &population_vec
){
  population_vec.reserve(population_vec.size()+count);
  for(int i=0;i<count;i++){
    population_vec.push_back(parent);    //Copy parent into the vector
    auto &child = population_vec.back(); //Get reference to
    for(auto &kv: child.has_charger){
      if(r_uniform(eng,0,1)<mutation_rate){ //If we mutate
        kv.second = !kv.second;             //Flip the bit
      }
    }
  }
}

ModelResults get_initial_entity(const ModelInfo &model_info){
  ModelResults temp;
  for(const auto &t: model_info.trips){
    temp.has_charger[t.start_stop_id] = false;
    temp.has_charger[t.end_stop_id]   = false;
  }
  return temp;
}

bool cost_compare(const ModelResults &a, const ModelResults &b){
  return a.cost<b.cost;
}

void optimization_stage_run(
  const ModelInfo &model_info,
  const size_t generation,
  our_random_engine &eng,
  results_vec &results
){
  const auto generations   = model_info.params.generations.at(generation);
  const auto mutation_rate = model_info.params.mutation_rate.at(generation);
  const auto keep_top      = model_info.params.keep_top.at(generation);
  const auto spawn_size    = model_info.params.spawn_size.at(generation);

  for(int g=0;g<generations;g++){
    std::cerr<<(g%10)<<std::flush;
    if(g%100==0)
      std::cerr<<std::endl;
    #pragma omp parallel for
    for(size_t i=0;i<results.size();i++){
      run_model(model_info, results.at(i));
      results.at(i).trips.clear(); //Save memory
    }
    if(static_cast<int>(results.size())>keep_top){
      std::nth_element(results.begin(), results.begin()+keep_top, results.end(), cost_compare);
      results.erase(results.begin()+keep_top,results.end());
    }
    const auto pre_spawn_size = results.size();
    for(int i=0;i<keep_top && (size_t)i<pre_spawn_size;i++){
      spawn_from(results.at(i), spawn_size, mutation_rate, eng, results);
    }
  }
  std::cerr<<std::endl;
}

ModelResults optimization_restart_run(
  const ModelInfo &model_info,
  our_random_engine &eng
){
  results_vec results;

  results.push_back(get_initial_entity(model_info));

  for(size_t g=0;g<model_info.params.generations.size();g++){
    std::cerr<<"Stage "<<g<<std::endl;
    optimization_stage_run(model_info, g, eng, results);
  }

  return *std::max_element(results.begin(), results.begin()+model_info.params.keep_top.back(), cost_compare);
}

ModelResults optimize_model(const ModelInfo &model_info){
  our_random_engine eng;

  seed_rand(eng, model_info.params.seed);

  results_vec results;

  for(int r=0;r<model_info.params.restarts;r++){
    results.push_back(optimization_restart_run(model_info, eng));
  }

  auto best = *std::max_element(results.begin(), results.end(), cost_compare);

  //Rerun best model to get the full results
  run_model(model_info, best);

  return best;
}
