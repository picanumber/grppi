/*
 * Copyright 2018 Universidad Carlos III de Madrid
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef GRPPI_NATIVE_PARALLEL_EXECUTION_DIST_TASK_H
#define GRPPI_NATIVE_PARALLEL_EXECUTION_DIST_TASK_H

#include "dist_pool.h"
#include "zmq_port_service.h"
#include "zmq_data_service.h"

#include "../common/mpmc_queue.h"
#include "../common/iterator.h"
#include "../common/execution_traits.h"
#include "../common/configuration.h"

#include <memory>
#include <thread>
#include <atomic>
#include <algorithm>
#include <vector>
#include <type_traits>
#include <tuple>
#include <experimental/optional>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <boost/serialization/utility.hpp>


// TODO: use macro for enabling this feature + prepare cmake for it
#ifdef GRPPI_DCEX

#include "text_in_container.hpp"


template <typename T>
static constexpr bool is_text_container = std::is_same<std::decay_t<T>, aspide::text_in_container>::value;

template <typename T>
constexpr bool is_aspide_container =
   is_text_container<T>;

template <typename T>
constexpr bool is_not_container = !is_aspide_container<T>;

template <typename T>
using requires_container = std::enable_if_t<is_aspide_container<T>, long>;

template <typename T>
using requires_not_container = std::enable_if_t<is_not_container<T>, long>;

#endif

#undef COUT
#define COUT if (0) {std::ostringstream foo;foo
#undef ENDL
#define ENDL std::endl;std::cout << foo.str();}

//#define DEBUG

namespace grppi {


/** 
 \brief Native task-based parallel execution policy.
 This policy uses ISO C++ threads as implementation building block allowing
 usage in any ISO C++ compliant platform.
 */
template <typename Scheduler>
class parallel_execution_dist_task {
public:
  // Alias type for the scheduler and the task type
  using scheduler_type = Scheduler;
  using task_type = typename Scheduler::task_type;
  using data_ref_type = typename Scheduler::data_ref_type;

//  /**
//  \brief Default construct a task parallel execution policy.
//
//  Creates a parallel execution task object.
//
//  The concurrency degree is determined by the platform.
//
//  \note The concurrency degree is fixed to the hardware concurrency
//   degree.
//  */
//  parallel_execution_dist_task() noexcept:
//    config_{},
//    ordering_{config_.ordering()},
//    scheduler_{new Scheduler{}}
//  {
//  }
//  parallel_execution_dist_task(bool ordering) noexcept :
//    config_{},
//    ordering_{ordering},
//    scheduler_{new Scheduler{}}
//  {
//  }

/**
  \brief Constructs a task parallel execution policy.

  Creates a parallel execution task object selecting the scheduler, concurrency degree
  and ordering mode.

  \param scheduler Scheduler to use.
  \param concurrency_degree Number of threads used for parallel algorithms.
  \param ordering Whether ordered executions is enabled or disabled.
  */
  parallel_execution_dist_task(std::unique_ptr<Scheduler> scheduler) noexcept :
    config_{},
    ordering_{config_.ordering()},
    scheduler_{std::move(scheduler)}
  {
    //COUT <<"parallel_execution_dist_task: ordering_ = " << ordering_ << ENDL;
  }

  parallel_execution_dist_task(std::unique_ptr<Scheduler> scheduler,
                               bool ordering) noexcept :
    config_{},
    ordering_{ordering},
    scheduler_{std::move(scheduler)}
  {
  }

  parallel_execution_dist_task(const parallel_execution_dist_task & ex) = delete;

  /** 
  \brief Destroy a task parallel execution policy.

  */
  ~parallel_execution_dist_task(){
     // do nothing
  }

  /**
  \brief Enable ordering.
  */
  void enable_ordering() noexcept { ordering_=true; }

  /**
  \brief Disable ordering.
  */
  void disable_ordering() noexcept { ordering_=false; }

  /**
  \brief Is execution ordered.
  */
  bool is_ordered() const noexcept { return ordering_; }

//#ifdef DEBUG
  /**
  \brief Invoke \ref md_divide-conquer.
  \tparam Input Type used for the input problem.
  \tparam Divider Callable type for the divider operation.
  \tparam Predicate Callable type for the stop condition predicate.
  \tparam Solver Callable type for the solver operation.
  \tparam Combiner Callable type for the combiner operation.
  \param ex Sequential execution policy object.
  \param input Input problem to be solved.
  \param divider_op Divider operation.
  \param predicate_op Predicate operation.
  \param solver_op Solver operation.
  \param combine_op Combiner operation.
  */
  template <typename Input, typename Divider, typename Predicate, typename Solver, typename Combiner>
  auto divide_conquer(Input && input,
                      Divider && divide_op,
                      Predicate && predicate_op,
                      Solver && solve_op,
                      Combiner && combine_op) const;
//#endif
  
  /**
  \brief Invoke \ref md_pipeline.
  \tparam Generator Callable type for the generator operation.
  \tparam Transformers Callable types for the transformers in the pipeline.
  \param generate_op Generator operation.
  \param transform_ops Transformer operations.
  */
  template <typename Generator, typename ... Transformers
#ifdef GRPPI_DCEX
	  , requires_not_container<Generator> = 0
#endif 
	  >
  void pipeline(Generator && generate_op, 
                Transformers && ... transform_ops) const;


#ifdef GRPPI_DCEX
  template <typename ... Transformers>
  void pipeline(aspide::text_in_container & container, Transformers && ... transform_ops) const;
#endif 

//  /**
//  \brief Invoke \ref md_pipeline coming from another context
//  that uses mpmc_queues as communication channels.
//  \tparam InputType Type of the input stream.
//  \tparam Transformers Callable types for the transformers in the pipeline.
//  \tparam InputType Type of the output stream.
//  \param input_queue Input stream communicator.
//  \param transform_ops Transformer operations.
//  \param output_queue Input stream communicator.
//  */
//  template <typename InputType, typename Transformer, typename OutputType>
//  void pipeline(std::shared_ptr<mpmc_queue<InputType>> & input_queue, Transformer && transform_op,
//                std::shared_ptr<mpmc_queue<OutputType>> &output_queue) const
//  {
//    do_pipeline(input_queue, std::forward<Transformer>(transform_op), output_queue);
//  }

private:

  template <typename InputItemType, typename Consumer,
            requires_no_pattern<Consumer> = 0>
  void do_pipeline(bool is_farm, Consumer && consume_op) const;

  template <typename InputItemType, typename Transformer,
            typename ... OtherTransformers,
            requires_no_pattern<Transformer> = 0>
  void do_pipeline(bool is_farm, Transformer && transform_op,
      OtherTransformers && ... other_ops) const;
    
    
  template <typename InputItemType, typename Transformer,
            requires_no_pattern<Transformer> = 0>
  void do_pipeline(bool is_farm, Transformer && transform_op, bool check) const;


  template <typename InputItemType, typename FarmTransformer,
            template <typename> class Farm,
            requires_farm<Farm<FarmTransformer>> = 0>
  void do_pipeline(bool is_farm, Farm<FarmTransformer> & farm_obj) const
  {
    do_pipeline<InputItemType>(is_farm, std::move(farm_obj));
  }

  template <typename InputItemType, typename FarmTransformer,
            template <typename> class Farm,
            requires_farm<Farm<FarmTransformer>> = 0>
  void do_pipeline(bool is_farm, Farm<FarmTransformer> && farm_obj) const;

  template <typename InputItemType, typename FarmTransformer,
            template <typename> class Farm,
            typename ... OtherTransformers,
            requires_farm<Farm<FarmTransformer>> = 0>
  void do_pipeline(
      bool is_farm, Farm<FarmTransformer> & farm_obj,
      OtherTransformers && ... other_transform_ops) const
  {
    do_pipeline<InputItemType>(is_farm, std::move(farm_obj),
        std::forward<OtherTransformers>(other_transform_ops)...);
  }

  template <typename InputItemType, typename FarmTransformer,
            template <typename> class Farm,
            typename ... OtherTransformers,
            requires_farm<Farm<FarmTransformer>> = 0>
  void do_pipeline(
      bool is_farm, Farm<FarmTransformer> && farm_obj,
      OtherTransformers && ... other_transform_ops) const;

  template <typename InputItemType, typename Predicate,
            template <typename> class Filter,
            typename ... OtherTransformers,
            requires_filter<Filter<Predicate>> =0>
  void do_pipeline(
      bool is_farm, Filter<Predicate> & filter_obj,
      OtherTransformers && ... other_transform_ops) const
  {
    do_pipeline<InputItemType>(is_farm, std::move(filter_obj),
        std::forward<OtherTransformers>(other_transform_ops)...);
  }

  template <typename InputItemType, typename Predicate,
            template <typename> class Filter,
            typename ... OtherTransformers,
            requires_filter<Filter<Predicate>> =0>
  void do_pipeline(
      bool is_farm, Filter<Predicate> && farm_obj,
      OtherTransformers && ... other_transform_ops) const;

  template <typename InputItemType, typename Combiner, typename Identity,
            template <typename C, typename I> class Reduce,
            typename ... OtherTransformers,
            requires_reduce<Reduce<Combiner,Identity>> = 0>
  void do_pipeline(bool is_farm, Reduce<Combiner,Identity> & reduce_obj,
                   OtherTransformers && ... other_transform_ops) const
  {
    do_pipeline<InputItemType>(is_farm, std::move(reduce_obj),
        std::forward<OtherTransformers>(other_transform_ops)...);
  }

  template <typename InputItemType, typename Combiner, typename Identity,
            template <typename C, typename I> class Reduce,
            typename ... OtherTransformers,
            requires_reduce<Reduce<Combiner,Identity>> = 0>
  void do_pipeline(bool is_farm, Reduce<Combiner,Identity> && reduce_obj,
                   OtherTransformers && ... other_transform_ops) const;

  template <typename InputItemType, typename Transformer, typename Predicate,
            template <typename T, typename P> class Iteration,
            typename ... OtherTransformers,
            requires_iteration<Iteration<Transformer,Predicate>> =0,
            requires_no_pattern<Transformer> =0>
  void do_pipeline(bool is_farm, Iteration<Transformer,Predicate> & iteration_obj,
                   OtherTransformers && ... other_transform_ops) const
  {
    do_pipeline<InputItemType>(is_farm, std::move(iteration_obj),
        std::forward<OtherTransformers>(other_transform_ops)...);
  }

  template <typename InputItemType, typename Transformer, typename Predicate,
            template <typename T, typename P> class Iteration,
            typename ... OtherTransformers,
            requires_iteration<Iteration<Transformer,Predicate>> =0,
            requires_no_pattern<Transformer> =0>
  void do_pipeline(bool is_farm, Iteration<Transformer,Predicate> && iteration_obj,
                   OtherTransformers && ... other_transform_ops) const;

  template <typename InputItemType, typename Transformer, typename Predicate,
            template <typename T, typename P> class Iteration,
            typename ... OtherTransformers,
            requires_iteration<Iteration<Transformer,Predicate>> =0,
            requires_pipeline<Transformer> =0>
  void do_pipeline(bool is_farm, Iteration<Transformer,Predicate> && iteration_obj,
                   OtherTransformers && ... other_transform_ops) const;


  template <typename InputItemType, typename ... Transformers,
            template <typename...> class Pipeline,
            requires_pipeline<Pipeline<Transformers...>> = 0>
  void do_pipeline(
      bool is_farm, Pipeline<Transformers...> & pipeline_obj) const
  {
    do_pipeline<InputItemType>(is_farm, std::move(pipeline_obj));
  }

  template <typename InputItemType, typename ... Transformers,
            template <typename...> class Pipeline,
            requires_pipeline<Pipeline<Transformers...>> = 0>
  void do_pipeline(
      bool is_farm, Pipeline<Transformers...> && pipeline_obj) const;

  template <typename InputItemType, typename ... Transformers,
            template <typename...> class Pipeline,
            typename ... OtherTransformers,
            requires_pipeline<Pipeline<Transformers...>> = 0>
  void do_pipeline(
      bool is_farm, Pipeline<Transformers...> & pipeline_obj,
      OtherTransformers && ... other_transform_ops) const
  {
    do_pipeline<InputItemType>(is_farm, std::move(pipeline_obj),
        std::forward<OtherTransformers>(other_transform_ops)...);
  }

  template <typename InputItemType, typename ... Transformers,
            template <typename...> class Pipeline,
            typename ... OtherTransformers,
            requires_pipeline<Pipeline<Transformers...>> = 0>
  void do_pipeline(
      bool is_farm, Pipeline<Transformers...> && pipeline_obj,
      OtherTransformers && ... other_transform_ops) const;

  template <typename InputItemType, typename ... Transformers,
            std::size_t ... I>
  void do_pipeline_nested(
      bool is_farm, std::tuple<Transformers...> && transform_ops,
      std::index_sequence<I...>) const;

private: 
  configuration<> config_;
  
  bool ordering_;

  mutable std::shared_ptr<Scheduler> scheduler_;
   
};

/**
\brief Metafunction that determines if type E is parallel_execution_dist_task
\tparam Execution policy type.
*/
template <typename E,typename T>
constexpr bool is_parallel_execution_dist_task() {
  return std::is_same<E, parallel_execution_dist_task<T> >::value;
}

template <typename T>
using execution_dist_task = parallel_execution_dist_task<T>;

/**
\brief Determines if an execution policy is supported in the current compilation.
\note Specialization for parallel_execution_dist_task.
*/
template <typename U>
struct support<parallel_execution_dist_task<U>>
{
   static constexpr bool value = true;
};

/*template <>
constexpr bool is_supported<execution_task>() { return true; }
*/
/**
\brief Determines if an execution policy supports the map pattern.
\note Specialization for parallel_execution_dist_task.
*/
/*template <>
constexpr bool supports_map<execution_task>() { return true; }
*/
/**
\brief Determines if an execution policy supports the reduce pattern.
\note Specialization for parallel_execution_dist_task.
*/
/*template <>
constexpr bool supports_reduce<parallel_execution_dist_task>() { return true; }
*/
/**
\brief Determines if an execution policy supports the map-reduce pattern.
\note Specialization for parallel_execution_dist_task.
*/
/*template <>
constexpr bool supports_map_reduce<parallel_execution_dist_task>() { return true; }
*/
/**
\brief Determines if an execution policy supports the stencil pattern.
\note Specialization for parallel_execution_dist_task.
*/
/*template <>
constexpr bool supports_stencil<parallel_execution_dist_task>() { return true; }
*/
/**
\brief Determines if an execution policy supports the divide/conquer pattern.
\note Specialization for parallel_execution_dist_task.
*/
/*template <>
constexpr bool supports_divide_conquer<parallel_execution_dist_task>() { return true; }
*/
/**
\brief Determines if an execution policy supports the pipeline pattern.
\note Specialization for parallel_execution_dist_task.
*/
/*template <>
constexpr bool supports_pipeline<parallel_execution_dist_task>() { return true; }
*/

//#ifdef DEBUG

template <typename Scheduler>
template <typename Input, typename Divider,typename Predicate, typename Solver, typename Combiner>
auto parallel_execution_dist_task<Scheduler>::divide_conquer(
    Input && input,
    Divider && divide_op,
    Predicate && predicate_op,
    Solver && solve_op,
    Combiner && combine_op) const
{
  constexpr sequential_execution seq;
  using result_type =
    std::decay_t<typename std::result_of<Solver(Input)>::type>;
  using input_type = std::decay_t<Input>;
  using data_type = std::pair < input_type, result_type >;

  COUT << "parallel_execution_dist_task::DIVIDE_CONQUER" << ENDL;

  // init divide task identifier
  long init_divide_id = 0;
  // normal divide task identifier
  long normal_divide_id = 0;
  // Merge task identifier
  long merger_id = 0;
  // Ending task identifier
  long ending_id = 0;

  auto merge_function =
  [&combine_op,this](task_type &task)
  {
    auto task_before_dep = task.get_before_dep();
    auto it = task_before_dep.begin();
    COUT <<"parallel_execution_dist_task::divide_conquer:MERGE(" << task.get_task_id() << "): BEGIN: taskid=" << task.get_task_id() << " dep={" << *(it++) << ", " << *(it) << "}" << ENDL;
    auto task_data_loc = task.get_data_location();
    // get final task output data as initial value
    auto data = scheduler_->template get<data_type>(task_data_loc[0]);
    // combine all partial output data from the tasks this was dependant
    for (long i=1; i<task_data_loc.size(); i++) {
      COUT<<"parallel_execution_dist_task::divide_conquer:MERGE(" << task.get_task_id() << "): iter=" << i << ENDL;
      // get partial data
      auto input = scheduler_->template get_release<data_type>(task_data_loc[i]);
      // combine partial data with final data
      data.second = combine_op(data.second, input.second);
    }
    COUT<<"parallel_execution_dist_task::divide_conquer:MERGE(" << task.get_task_id() << "): final result=" << data.second << ENDL;
    
    // store final output data
    scheduler_->set(data,task_data_loc[0]);
    
    // finish this branch and free the tokens of the PREVIOUS data locs
    // but DO NOT FREE the resulting data loc that would be used latter
    scheduler_->finish_task(task, (task_data_loc.size()-1));
    
    COUT<<"parallel_execution_dist_task::divide_conquer:MERGE(" << task.get_task_id() << "): END" << ENDL;
    return;
  };

  auto normal_divide_function =
  [&divide_op, &predicate_op, &solve_op, &combine_op, &seq, &merger_id, &normal_divide_id, this](task_type &task)
  {
    COUT<<"parallel_execution_dist_task::divide_conquer:DIVIDE(" << task.get_task_id() << "): BEGIN: taskid=" << task.get_task_id() <<
        " after_dep count = " << task.get_after_dep().size() << " after_dep (0) = " << *(task.get_after_dep().begin()) <<
        " before_dep count = " << task.get_before_dep().size() << " before_dep (0) = " << *(task.get_before_dep().begin()) << ENDL;
    // get" in/out data for this problem
    auto task_data_loc = task.get_data_location();
    auto data = scheduler_->template get<data_type>(task_data_loc[0]);
    // check if this problem is a simple one
    if( predicate_op(data.first) ) {
      COUT<<"parallel_execution_dist_task::divide_conquer:DIVIDE(" << task.get_task_id() << "): SIMPLE CASE: input = " << data.first << ENDL;
      // solve the simple problem
      data.second = solve_op(data.first);
      // save output data on the allocated data loc
      scheduler_->set(data,task_data_loc[0]);
    
      // finish this branch but DO NOT FREE the token (data loc will be used)
      scheduler_->finish_task(task, 0);
    
      COUT<<"parallel_execution_dist_task::divide_conquer:DIVIDE(" << task.get_task_id() << "): SIMPLE CASE: END" << ENDL;
      return;
    }
    // obtain all the direct subproblems
    auto subproblems = divide_op(data.first);
    // if there are tokens for all subproblems -> allocate them
    if( false == scheduler_->allocate_tokens(subproblems.size()) ) {
      COUT<<"parallel_execution_dist_task::divide_conquer:DIVIDE(" << task.get_task_id() << "): SEQUENCIAL CASE: input = " << data.first << ENDL;
      // if not enough tokens solve the problem sequentally
      data.second = seq.divide_conquer(data.first,
             std::forward<Divider>(divide_op), std::forward<Predicate>(predicate_op),
      std::forward<Solver>(solve_op), std::forward<Combiner>(combine_op) );
      // save output data on the allocated data loc
      scheduler_->set(data,task_data_loc[0]);

      // finish this branch but DO NOT FREE the token (data loc will be used)
      scheduler_->finish_task(task, 0);

      COUT<<"parallel_execution_dist_task::divide_conquer:DIVIDE(" << task.get_task_id() << "): SEQUENCIAL CASE: END" << ENDL;
      return;
    }
    COUT<<"parallel_execution_dist_task::divide_conquer:DIVIDE(" << task.get_task_id() << "): PARALLEL CASE: input = " << data.first << ENDL;

    // create merger task and its auxiliar data loc vector and dependency set
    task_type merger_task{merger_id, task.get_task_id(), task.get_order(),
                          task.get_local_ids(), task.get_is_hard()};
    merger_task.set_after_dep(task.get_after_dep());
    auto merger_data_loc = task.get_data_location();
    std::set<long> merger_task_before_dep;
    
    // create a new task for each subproblem
    for( unsigned long i = 0; i < subproblems.size(); i++){
      //Store data in data manager
      //auto ref = scheduler_->set(std::make_pair(subproblems[i], result_type{}));
      auto ref = scheduler_->set(data_type{subproblems[i], result_type{}});
      // create new task for subproblem
      task_type new_task {normal_divide_id, scheduler_->get_task_id(), task.get_order(), {scheduler_->get_node_id()}, false, {ref}};
      std::set<long> aux_set{};
      aux_set.insert(merger_task.get_task_id());
      new_task.set_after_dep(aux_set);
      COUT<<"parallel_execution_dist_task::divide_conquer:DIVIDE(" << task.get_task_id() << "): new task: taskid=" << new_task.get_task_id()  << " after_dep={" << *(new_task.get_after_dep().begin()) << "}" << ENDL;
      // update merge task with the new dependency and data loc
      merger_task_before_dep.insert(new_task.get_task_id());
      merger_data_loc.push_back(ref);
      //Launch the task
      scheduler_->set_task(new_task,false);
    }
    // finish and send merge task with its data locs and dependency set
    {std::string aux_str;
    for (auto it=merger_task_before_dep.begin(); it!=merger_task_before_dep.end();it++) {
        aux_str+=std::to_string(*it);
        aux_str+=",";
    }
    COUT<<"parallel_execution_dist_task::divide_conquer:DIVIDE(" << task.get_task_id() << "): merger task: taskid=" << merger_task.get_task_id()  << " dep={" << aux_str << "}" << ENDL;}
    merger_task.set_data_location(merger_data_loc);
    merger_task.set_before_dep(merger_task_before_dep);
    scheduler_->set_task(merger_task,false);
    COUT<<"parallel_execution_dist_task::divide_conquer:DIVIDE(" << task.get_task_id() << "): PARALLEL CASE: END" << ENDL;
    return;
  };

  auto init_divide_function = [&normal_divide_function, &ending_id, input, this](task_type &task)
  {
    COUT<<"parallel_execution_dist_task::divide_conquer:INITIAL(" << task.get_task_id() << "): BEGIN: taskid=" << task.get_task_id()  << ENDL;
    // store initial input on the local data server
    //auto ref = scheduler_->set(std::make_pair(input,result_type{}));
    auto ref = scheduler_->set(data_type{input,result_type{}});
    
    // update task with input storing information
    task.set_data_location({ref});
    task.set_local_ids({scheduler_->get_node_id()});
    task.set_is_hard(false);
    
    // create ending task
    task_type end_task {ending_id, scheduler_->get_task_id(), task.get_order(), {scheduler_->get_node_id()}, false, {ref}};

    // set before task dependency of initial task in ending task
    end_task.set_before_dep({task.get_task_id()});
    // set end task dependency on initial task
    task.set_after_dep({end_task.get_task_id()});

    //Launch the ending task
    scheduler_->set_task(end_task,true);
    
    COUT<<"parallel_execution_dist_task::divide_conquer:INITIAL(" << task.get_task_id() << "): Launched ending task(" << end_task.get_task_id() << ")" << ENDL;

    // solve the task problem
    normal_divide_function(task);
    COUT<<"parallel_execution_dist_task::divide_conquer:INITIAL(" << task.get_task_id() << "): END" << ENDL;
    return;
  };

  auto ending_function = [this](task_type &task)
  {
    COUT<<"parallel_execution_dist_task::divide_conquer:ENDING(" << task.get_task_id() << "): BEGIN: taskid=" << task.get_task_id()  << ENDL;
        // finish task free this token and the previous one
        scheduler_->finish_task(task,2);
    COUT<<"parallel_execution_dist_task::divide_conquer:ENDING(" << task.get_task_id() << "): END" << ENDL;
    return;
  };

  init_divide_id = scheduler_->register_parallel_task(std::move(init_divide_function), true);
  normal_divide_id = scheduler_->register_parallel_task(std::move(normal_divide_function), true);
  merger_id = scheduler_->register_parallel_task(std::move(merge_function), false);
  ending_id = scheduler_->register_parallel_task(std::move(ending_function), false);

  auto task = scheduler_->run();
  COUT<<"parallel_execution_dist_task::divide_conquer: run DONE: taskid=" << task.get_task_id() << ENDL;

  auto result = scheduler_->template get_release_all<data_type>(task.get_data_location()[0]);
  return result.second;
}
//#endif

#ifdef GRPPI_DCEX
template <typename Scheduler>
template <typename ... Transformers>
void parallel_execution_dist_task<Scheduler>::pipeline(
		aspide::text_in_container & container, Transformers && ... transform_ops) const
{
  using namespace std;
  using output_type = pair<std::string,long>;
  long order = 0;
  // Local container
  if(container.type == 0 ) {
     scheduler_->register_sequential_task([&container, this, &order](task_type &t) -> void
      {
         auto iterator = container.begin_f();
	 // Obtain files in the container
	 while(iterator != container.end_f()){
             auto file = *(iterator);
	     auto file_iter = file.begin();
	     // Obtain each string part of the file
	     while(file_iter != file.end()){
                 auto value = scheduler_->set(make_pair(*file_iter, order));
		 //Creates a task for each data item
                 task_type next_task{t.get_id()+1, scheduler_->get_task_id(), order, {scheduler_->get_node_id()}, false, {value}};
                 scheduler_->set_task(next_task,true);
		 order++;
		 ++file_iter;

	     }
	     ++iterator;
	 }
	 scheduler_->finish_task(t);
 
      }, true);
   
   }
   // TODO: parallel filesystems, hope they may work in the same way for any underlaying filesystem
   else{
      scheduler_->register_sequential_task([&container, this, &order](task_type &t) -> void
      {
        long num_files = container.size();
        auto file_iter = container.begin_f();
	for(long  i = 0; i<num_files; i++){
            auto file = *(file_iter);
	    // Those are the machines that have access to the file. 
	    std::array<std::string,3> data_loc= file.get_data_location(); 
	    // Transform to node ids 
	    // Compare with machine list (scheduler_->get_machine_nodes(std::array<std::string,3>)))
	    // 
            auto value = scheduler_->set(make_pair(i, order));
            task_type next_task{t.get_id()+1, scheduler_->get_task_id(), order, 
		                {scheduler_->get_node_id()}/*TODO*/, false /*TODO: HARD OR SOFT*/, {value}};
            scheduler_->set_task(next_task,true);
            ++file_iter;
	    order++;
	}
	scheduler_->finish_task(t);
      }, true);

      scheduler_->register_parallel_task([&container, this](task_type &t) -> void
      {
         //TODO: We probably need to modify the order information for ordering items inside a file
        long order = 0;

        auto item = scheduler_->template get_release<pair<long,long>>(t.get_data_location()[0]);
	    auto curr_file = container.begin_f() + item.first;
	    auto file = *(curr_file);
	    auto file_iter = file.begin();
	    while(file_iter != file.end()) {
             auto value = scheduler_->set(make_pair(*file_iter, order));
	         //Creates a task for each data item
             task_type next_task{t.get_id()+1, scheduler_->get_task_id(), order, 
		                {scheduler_->get_node_id()}/*TODO*/, false /*TODO: HARD OR SOFT*/, {value}};
             scheduler_->set_task(next_task,true);
	        order++;
	        ++file_iter;
	    }
	    scheduler_->finish_task(t);
      }, true);
       COUT<<"parallel_execution_dist_task::pipeline(container...): NOT SUPPORTED"<<ENDL;

       return;
   }
   do_pipeline<output_type>(false, forward<Transformers>(transform_ops)...);

}
#endif

template <typename Scheduler>
template <typename Generator, typename ... Transformers
#ifdef GRPPI_DCEX
	  , requires_not_container<Generator>
#endif
	  >
void parallel_execution_dist_task<Scheduler>::pipeline(
    Generator && generate_op, 
    Transformers && ... transform_ops) const
{
  using namespace std;
  using result_type = decay_t<typename result_of<Generator()>::type>;
  using output_type = pair<typename result_type::value_type,long>;

  long order=0;
  COUT << "parallel_execution_dist_task::pipeline(GENERATOR): is_farm = 0" << ENDL;

  std::function<void(task_type&)> task_func([&generate_op, this, &order](task_type &t) -> void
  {
     COUT << "parallel_execution_dist_task::pipeline(GENERATOR): task["<< t.get_id() << ","<< t.get_task_id()<< "]: generator, ref=(" << t.get_data_location()[0].get_id() << "," << t.get_data_location()[0].get_pos() << ")" << ENDL;
#ifdef DEBUG
     std::vector<task_type> conf_tasks;
     auto item{generate_op(conf_tasks)};
#else
     auto item{generate_op()};
#endif
     if(item){
       auto ref = scheduler_->set(make_pair(*item, order));
       //COUT << "parallel_execution_dist_task::pipeline(GENERATOR): task["<< t.get_id() << ","<< t.get_task_id()<< "]: generator, launch task[" << t.get_id()+1 <<"," << order << "] ref=(" << ref.get_id() << "," << ref.get_pos() << ")" << ENDL;
#ifdef DEBUG
       task_type next_task{t.get_id()+1, scheduler_->get_task_id(), order, conf_tasks[1].get_local_ids(), conf_tasks[1].get_is_hard(), {ref}};
#else
       task_type next_task{t.get_id()+1, scheduler_->get_task_id(), order, {scheduler_->get_node_id()}, false, {ref}};
#endif
       scheduler_->set_task(next_task,false);
       
       // increase order
       order++;
       //COUT << "parallel_execution_dist_task::pipeline(GENERATOR): task["<< t.get_id() << ","<< t.get_task_id()<< "]: generator, launch task[" << t.get_id() << ","<< order << "], ref=(" << t.get_data_location()[0].get_id() << "," << t.get_data_location()[0].get_pos() << ")" << ENDL;
#ifdef DEBUG
       task_type gen_task{t.get_id(), scheduler_->get_task_id(), order, conf_tasks[0].get_local_ids(), conf_tasks[0].get_is_hard()};
#else
       task_type gen_task{t.get_id(), scheduler_->get_task_id(), order, {scheduler_->get_node_id()}, false};
#endif
       scheduler_->set_task(gen_task,true);
     } else {
       //COUT << "parallel_execution_dist_task::pipeline(GENERATOR): task: generator item = false" << ENDL;
       scheduler_->finish_task(t);
     }
  });
  scheduler_->register_sequential_task(std::move(task_func), true);

  do_pipeline<output_type>(false, forward<Transformers>(transform_ops)...);
}

// PRIVATE MEMBERS
template <typename Scheduler>
template <typename InputItemType, typename Consumer,
          requires_no_pattern<Consumer>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Consumer && consume_op) const
{

  using namespace std;
  //TODO: Need to reimplement ordering
  COUT << "parallel_execution_dist_task::pipeline(...CONSUMER): is_farm = "<< is_farm << ENDL;
  std::function<void(task_type&)> task_func([&consume_op, this](task_type t) -> void
  {
     COUT << "parallel_execution_dist_task::pipeline(...CONSUMER): task["<< t.get_id() << ","<< t.get_task_id()<< "]: consumer, ref=(" << t.get_data_location()[0].get_id() << "," << t.get_data_location()[0].get_pos() << ")" << ENDL;
     auto item = scheduler_->template get_release<InputItemType>(t.get_data_location()[0]);
     consume_op(item.first);
     scheduler_->finish_task(t);
  });
  scheduler_->register_sequential_task(std::move(task_func), false);
  scheduler_->run();
}

template <typename Scheduler>
template <typename InputItemType, typename Transformer,
          typename ... OtherTransformers,
          requires_no_pattern<Transformer>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Transformer && transform_op,
    OtherTransformers && ... other_transform_ops) const
{
  using namespace std;
  using namespace experimental;

  using input_item_value_type = typename InputItemType::first_type;
  using transform_result_type = 
      decay_t<typename result_of<Transformer(input_item_value_type)>::type>;
  using output_item_type = pair<transform_result_type,long>;

  COUT << "parallel_execution_dist_task::pipeline(.NO PATTERN.): is_farm = "<< is_farm << ENDL;

  std::function<void(task_type&)> task_func([this,&transform_op](task_type t) -> void
  {
    COUT << "parallel_execution_dist_task::pipeline(.NO PATTERN.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: no_pattern, ref=(" << t.get_data_location()[0].get_id() << "," << t.get_data_location()[0].get_pos() << ")" << ENDL;
    auto item = scheduler_->template get_release<InputItemType>(t.get_data_location()[0]);
#ifdef DEBUG
     std::vector<task_type> conf_tasks;
     auto out = transform_op(item.first, conf_tasks);
#else
     auto out = transform_op(item.first);
#endif
    auto ref = scheduler_->set(make_pair(out,item.second));
    //COUT << "parallel_execution_dist_task::pipeline(.NO PATTERN.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: no_pattern, launch task[" << t.get_id()+1 <<"," << t.get_task_id() << "] ref=(" << ref.get_id() << "," << ref.get_pos() << ")" << ENDL;
#ifdef DEBUG
    task_type next_task{t.get_id()+1, scheduler_->get_task_id(), t.get_order(), conf_tasks[1].get_local_ids(), conf_tasks[1].get_is_hard(), {ref}};
#else
    task_type next_task{t.get_id()+1, scheduler_->get_task_id(), t.get_order(), {scheduler_->get_node_id()}, false, {ref}};
#endif
    scheduler_->set_task(next_task,false);
  });
  
  if (is_farm) {
    scheduler_->register_parallel_task(std::move(task_func),false);
  } else {
    scheduler_->register_sequential_task(std::move(task_func), false);
  }

  do_pipeline<output_item_type>(is_farm, forward<OtherTransformers>(other_transform_ops)...);
}

template <typename Scheduler>
template <typename InputItemType, typename Transformer,
            requires_no_pattern<Transformer>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(bool is_farm, Transformer && transform_op, bool check) const
{
  using namespace std;
  using namespace experimental;
  
  if (!check) {
    return;
  }
  
  COUT << "parallel_execution_dist_task::pipeline(.NO PATTERN END.): is_farm = "<< is_farm << ENDL;
  
  std::function<void(task_type&)> task_func([this, &transform_op](task_type t) -> void
  {
    COUT << "parallel_execution_dist_task::pipeline(.NO PATTERN END.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: no_pattern_end, ref=(" << t.get_data_location()[0].get_id() << "," << t.get_data_location()[0].get_pos() << ")" << ENDL;
    auto item = scheduler_->template get_release<InputItemType>(t.get_data_location()[0]);
#ifdef DEBUG
    std::vector<task_type> conf_tasks;
    auto out = transform_op(item.first, conf_tasks);
#else
    auto out = transform_op(item.first);
#endif
    auto ref = scheduler_->set(make_pair(out,item.second));
    //COUT << "parallel_execution_dist_task::pipeline(.NO PATTERN END.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: no_pattern_farm, launch task[" << t.get_id()+1 <<"," << t.get_task_id() << "] ref=(" << ref.get_id() << "," << ref.get_pos() << ")" << ENDL;
#ifdef DEBUG
    task_type next_task{t.get_id()+1, scheduler_->get_task_id(), t.get_order(), conf_tasks[1].get_local_ids(), conf_tasks[1].get_is_hard(), {ref}};
#else
    task_type next_task{t.get_id()+1, scheduler_->get_task_id(), t.get_order(), {scheduler_->get_node_id()}, false, {ref}};
#endif
    scheduler_->set_task(next_task,false);
  });

  if (is_farm) {
    scheduler_->register_parallel_task(std::move(task_func),false);
  } else {
    scheduler_->register_sequential_task(std::move(task_func), false);
  }
}

template <typename Scheduler>
template <typename InputItemType, typename FarmTransformer,
          template <typename> class Farm,
          requires_farm<Farm<FarmTransformer>>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Farm<FarmTransformer> && farm_obj) const
{
  using namespace std;
  
  
  COUT << "parallel_execution_dist_task::pipeline(...FARM CONSUMER): is_farm = " << is_farm << ENDL;
  std::function<void(task_type&)> task_func([this,&farm_obj](task_type t) -> void
  {
    COUT << "parallel_execution_dist_task::pipeline(...FARM CONSUMER): task["<< t.get_id() << ","<< t.get_task_id()<< "]: farm consumer, ref=(" << t.get_data_location()[0].get_id() << "," << t.get_data_location()[0].get_pos() << ")" << ENDL;
    auto item = scheduler_->template get_release<InputItemType>(t.get_data_location()[0]);
    farm_obj(item.first);
    scheduler_->finish_task(t);
  });
  scheduler_->register_parallel_task(std::move(task_func),false);
  scheduler_->run();

}

template <typename Scheduler>
template <typename InputItemType, typename FarmTransformer,
          template <typename> class Farm,
          typename ... OtherTransformers,
          requires_farm<Farm<FarmTransformer>>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Farm<FarmTransformer> && farm_obj,
    OtherTransformers && ... other_transform_ops) const
{
  using namespace std;
  using namespace experimental;

  using input_item_value_type = typename InputItemType::first_type;

  using output_type = typename stage_return_type<input_item_value_type, FarmTransformer>::type;
  using output_item_type = pair <output_type, long> ;

  //COUT << "FARM" << ENDL;
  do_pipeline<InputItemType>(true, farm_obj.transformer(),true);
  do_pipeline<output_item_type>(is_farm, forward<OtherTransformers>(other_transform_ops)... );
}

template <typename Scheduler>
template <typename InputItemType, typename Predicate,
          template <typename> class Filter,
          typename ... OtherTransformers,
          requires_filter<Filter<Predicate>>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Filter<Predicate> && filter_obj,
    OtherTransformers && ... other_transform_ops) const
{
  using namespace std;
  using namespace experimental;

  COUT << "parallel_execution_dist_task::pipeline(.FILTER.): is_farm = "<< is_farm << ENDL;
  std::function<void(task_type&)> task_func([&filter_obj, this](task_type &t) -> void
  {
      COUT << "parallel_execution_dist_task::pipeline(.FILTER.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: filter, ref=(" << t.get_data_location()[0].get_id() << "," << t.get_data_location()[0].get_pos() << ")" << ENDL;
      auto item = scheduler_->template get_release<InputItemType>(t.get_data_location()[0]);
      if (filter_obj(item.first)) {
        auto ref = scheduler_->set(item);
        //COUT << "parallel_execution_dist_task::pipeline(.FILTER.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: filter, launch task[" << t.get_id()+1 <<"," << t.get_task_id() << "] ref=(" << ref.get_id() << "," << ref.get_pos() << ")" << ENDL;
        task_type next_task{t.get_id()+1, scheduler_->get_task_id(), t.get_order(), {scheduler_->get_node_id()}, false, {ref}};
        scheduler_->set_task(next_task,false);
      } else {
        //COUT << "parallel_execution_dist_task::pipeline(.FILTER.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: filter is consumed" << ENDL;
        scheduler_->finish_task(t);
      }
  });
  if (is_farm) {
    scheduler_->register_parallel_task(std::move(task_func),false);
  } else {
    scheduler_->register_sequential_task(std::move(task_func), false);
  }
  do_pipeline<InputItemType>(is_farm, forward<OtherTransformers>(other_transform_ops)...);

}

template <typename Scheduler>
template <typename InputItemType, typename Combiner, typename Identity,
          template <typename C, typename I> class Reduce,
          typename ... OtherTransformers,
          requires_reduce<Reduce<Combiner,Identity>>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Reduce<Combiner,Identity> && reduce_obj,
    OtherTransformers && ... other_transform_ops) const
{
  using namespace std;
  using namespace experimental;

  using output_item_value_type = decay_t<Identity>;
  using output_item_type = pair<output_item_value_type,long>;

  // Review if it can be transformed into parallel task
  // Transform into atomic if used as a parallel task
  long long order = 0;

  COUT << "parallel_execution_dist_task::pipeline(.REDUCE.): is_farm = "<< is_farm << ENDL;
  std::function<void(task_type&)> task_func([&reduce_obj, this, &order](task_type t) -> void
  {
    COUT << "parallel_execution_dist_task::pipeline(.REDUCE.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: reduce, ref=(" << t.get_data_location()[0].get_id() << "," << t.get_data_location()[0].get_pos() << ")" << ENDL;
    auto item = scheduler_->template get_release<InputItemType>(t.get_data_location()[0]);
    reduce_obj.add_item(std::forward<Identity>(item.first));
    if(reduce_obj.reduction_needed()) {
      constexpr sequential_execution seq;
      auto red = reduce_obj.reduce_window(seq);
      auto ref = scheduler_->set(make_pair(red, order++));
      //COUT << "parallel_execution_dist_task::pipeline(.REDUCE.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: reduce, launch task[" << t.get_id()+1 <<"," << t.get_task_id() << "] ref=(" << ref.get_id() << "," << ref.get_pos() << ")" << ENDL;
      scheduler_->set_task(task_type{t.get_id()+1, scheduler_->get_task_id(), t.get_order(), {scheduler_->get_node_id()}, false, {ref}},false);
    } else{
      scheduler_->finish_task(t);
    }
  });
  scheduler_->register_sequential_task(std::move(task_func), false);

  do_pipeline<output_item_type>(is_farm, forward<OtherTransformers>(other_transform_ops)...);
}

template <typename Scheduler>
template <typename InputItemType, typename Transformer, typename Predicate,
          template <typename T, typename P> class Iteration,
          typename ... OtherTransformers,
          requires_iteration<Iteration<Transformer,Predicate>>,
          requires_no_pattern<Transformer>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Iteration<Transformer,Predicate> && iteration_obj,
    OtherTransformers && ... other_transform_ops) const
{
  using namespace std;
  using namespace experimental;

  COUT << "parallel_execution_dist_task::pipeline(.ITERATION.): is_farm = "<< is_farm << ENDL;
  std::function<void(task_type&)> task_func([&iteration_obj, this](task_type t) -> void
  {
      COUT << "parallel_execution_dist_task::pipeline(.ITERATION.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: iteration, ref=(" << t.get_data_location()[0].get_id() << "," << t.get_data_location()[0].get_pos() << ")" << ENDL;
      auto item = scheduler_->template get_release<InputItemType>(t.get_data_location()[0]);
      auto value = iteration_obj.transform(item.first);
      auto new_item = InputItemType{value,item.second};
      if (iteration_obj.predicate(value)) {
        auto ref = scheduler_->set(new_item);
        //COUT << "parallel_execution_dist_task::pipeline(.ITERATION.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: iteration, launch task[" << t.get_id()+1 <<"," << t.get_task_id() << "] ref=(" << ref.get_id() << "," << ref.get_pos() << ")" << ENDL;
        task_type next_task{t.get_id()+1, scheduler_->get_task_id(), t.get_order(), {scheduler_->get_node_id()}, false, {ref}};
        scheduler_->set_task(next_task,false);
      }
      else {
        auto ref = scheduler_->set(new_item);
        t.set_data_location({ref});
        //COUT << "parallel_execution_dist_task::pipeline(.ITERATION.): task["<< t.get_id() << ","<< t.get_task_id()<< "]: iteration, launch task[" << t.get_id() <<"," << t.get_task_id() << "] ref=(" << ref.get_id() << "," << ref.get_pos() << ")" << ENDL;
        scheduler_->set_task(t,false);
      }
  });
  if (is_farm) {
    scheduler_->register_parallel_task(std::move(task_func),false);
  } else {
    scheduler_->register_sequential_task(std::move(task_func), false);
  }

  do_pipeline<InputItemType>(is_farm, forward<OtherTransformers>(other_transform_ops)...);
}

template <typename Scheduler>
template <typename InputItemType, typename Transformer, typename Predicate,
          template <typename T, typename P> class Iteration,
          typename ... OtherTransformers,
          requires_iteration<Iteration<Transformer,Predicate>>,
          requires_pipeline<Transformer>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Iteration<Transformer,Predicate> &&,
    OtherTransformers && ...) const
{
  static_assert(!is_pipeline<Transformer>, "Not implemented");
}

template <typename Scheduler>
template <typename InputItemType, typename ... Transformers,
          template <typename...> class Pipeline,
          requires_pipeline<Pipeline<Transformers...>>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Pipeline<Transformers...> && pipeline_obj) const
{
  //COUT << "parallel_execution_dist_task::pipeline(.PIPELINE 1.)" << ENDL;
  do_pipeline_nested<InputItemType>(
      is_farm, pipeline_obj.transformers(),
      std::make_index_sequence<sizeof...(Transformers)>());
}

template <typename Scheduler>
template <typename InputItemType, typename ... Transformers,
          template <typename...> class Pipeline,
          typename ... OtherTransformers,
          requires_pipeline<Pipeline<Transformers...>>>
void parallel_execution_dist_task<Scheduler>::do_pipeline(
    bool is_farm, Pipeline<Transformers...> && pipeline_obj,
    OtherTransformers && ... other_transform_ops) const
{
  //COUT << "parallel_execution_dist_task::pipeline(.PIPELINE 1-1.)" << ENDL;
  do_pipeline_nested<InputItemType>(
      is_farm, std::tuple_cat(pipeline_obj.transformers(),
          std::forward_as_tuple(other_transform_ops...)),
      std::make_index_sequence<sizeof...(Transformers)+sizeof...(OtherTransformers)>());
}

template <typename Scheduler>
template <typename InputItemType, typename ... Transformers,
          std::size_t ... I>
void parallel_execution_dist_task<Scheduler>::do_pipeline_nested(
    bool is_farm, std::tuple<Transformers...> && transform_ops,
    std::index_sequence<I...>) const
{
  //COUT << "parallel_execution_dist_task::pipeline(.PIPELINE 2.)" << ENDL;
  do_pipeline<InputItemType>(is_farm, std::forward<Transformers>(std::get<I>(transform_ops))...);
}

} // end namespace grppi

#endif
