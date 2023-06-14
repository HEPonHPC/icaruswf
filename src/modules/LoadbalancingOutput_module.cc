#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/OutputModule.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/ResultsPrincipal.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "canvas/Persistency/Common/Assns.h"
#include "canvas/Persistency/Common/Wrapper.h"
#include "canvas/Persistency/Provenance/canonicalProductName.h"
#include "fhiclcpp/types/ConfigurationTable.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include "hepnos.hpp"

#include "../serialization/cluster_serialization.h"
#include "../serialization/edge_serialization.h"
#include "../serialization/eid_serialization.h"
#include "../serialization/hit_serialization.h"
#include "../serialization/opdetwaveform_serialization.h"
#include "../serialization/opflash_serialization.h"
#include "../serialization/ophit_serialization.h"
#include "../serialization/pcaxis_serialization.h"
#include "../serialization/pfparticle_serialization.h"
#include "../serialization/rawdigit_serialization.h"
#include "../serialization/seed_serialization.h"
#include "../serialization/slice_serialization.h"
#include "../serialization/spacepoint_serialization.h"
#include "../serialization/vertex_serialization.h"
#include "../serialization/wire_serialization.h"

#include "EID.h"
#include "HepnosDataStore.h"
#include "canvas/Utilities/uniform_type_name.h"
#include <boost/serialization/utility.hpp>

#include "canvas/Utilities/FriendlyName.h"

using namespace art;
namespace hepnos {
  template <typename A, typename B>
  using Assns = std::vector<std::pair<hepnos::Ptr<A>, hepnos::Ptr<B>>>;
  template <typename A, typename B, typename D>
  using AssnsD = std::vector<std::tuple<hepnos::Ptr<A>, hepnos::Ptr<B>, D>>;
}

namespace {

  std::tuple<std::string, std::string, std::string>
  splitTag(std::string const& inputtag)
  {
    // InputTag: label = 'daq0', instance = 'PHYSCRATEDATATPCEE', process =
    // 'DetSim' For clarity I am defining all these variables
    art::InputTag a_inputtag;
    decode(inputtag, a_inputtag);

    std::string label = a_inputtag.label();
    std::string instance = a_inputtag.instance();
    std::string process = a_inputtag.process();
    return std::make_tuple(label, instance, process);
  }

  template <typename P>
  P const*
  prodWithType(art::EDProduct const* product, art::BranchDescription const& pd)
  {
    if (product == nullptr)
      return nullptr;
    if (pd.producedClassName() != art::TypeID{typeid(P)}.className())
      return nullptr;
    if (auto wp = dynamic_cast<art::Wrapper<P> const*>(product))
      return wp->product();
    return nullptr;
  }

  template <typename A, typename B>
  void
  storeassns(hepnos::DataStore datastore,
             hepnos::Event& h_e,
             std::map<art::ProductID, hepnos::ProductID> const& a_map,
             art::InputTag const& a_t,
             art::Assns<A, B> const& a_assns)
  {
    hepnos::Assns<A, B> h_assns;
    h_assns.reserve(a_assns.size());

    for (auto const& [a, b] : a_assns) {
      //  std::cout <<"storeassns, art prod keys: " << a.key() << ", " <<
      //  b.key() << std::endl; std::cout << "storeassns, art prod IDs: " <<
      //  a.id() << ", " << b.id() << std::endl;
      auto const A_ptr = datastore.makePtr<A>(a_map.at(a.id()), a.key());
      auto const B_ptr = datastore.makePtr<B>(a_map.at(b.id()), b.key());
      h_assns.emplace_back(A_ptr, B_ptr);
    }

    {
      std::function<void(void)> f = [&]() { h_e.store(a_t.encode(), h_assns); };
      art::ServiceHandle<icaruswf::HepnosDataStore>()->set_work_function(f);
      art::ServiceHandle<icaruswf::HepnosDataStore>()->set_work_state();
      art::ServiceHandle<icaruswf::HepnosDataStore>()->wait();
    }

    // std::cout << "Assns in art event: " << a_t.process() << ", " <<
    // a_t.label() << ", " << a_t.encode() << "\n";
  }

  template <typename A, typename B, typename D>
  void
  storeassns(hepnos::DataStore datastore,
             hepnos::Event& h_e,
             std::map<art::ProductID, hepnos::ProductID> const& a_map,
             art::InputTag const& a_t,
             art::Assns<A, B, D> const& a_assns)
  {
    hepnos::AssnsD<A, B, D> h_assns;
    h_assns.reserve(a_assns.size());
    for (auto const& [a, b, d] : a_assns) {
      auto const A_ptr = datastore.makePtr<A>(a_map.at(a.id()), a.key());
      auto const B_ptr = datastore.makePtr<B>(a_map.at(b.id()), b.key());
      h_assns.emplace_back(A_ptr, B_ptr, d);
    }

    {
      std::function<void(void)> f = [&]() { h_e.store(a_t, h_assns); };
      art::ServiceHandle<icaruswf::HepnosDataStore>()->set_work_function(f);
      art::ServiceHandle<icaruswf::HepnosDataStore>()->set_work_state();
      art::ServiceHandle<icaruswf::HepnosDataStore>()->wait();
    }

    // std::cout << "Assns in art event: " << a_t.process() << ", " <<
    // a_t.label() << ", " << a_t.encode() << "\n";
  }

  template <typename A, typename B, typename D = void>
  bool
  assnsWithType(art::EDProduct const* product,
                art::BranchDescription const& pd,
                hepnos::DataStore ds,
                hepnos::Event& h_e,
                std::map<art::ProductID, hepnos::ProductID> const& a_map)
  {
    using P = art::Assns<A, B, D>;
    if (product == nullptr)
      return false;
    if (pd.friendlyClassName() != art::TypeID{typeid(P)}.friendlyClassName())
      return false;
    if (auto wp = dynamic_cast<art::Wrapper<P> const*>(product)) {
      storeassns(ds, h_e, a_map, pd.inputTag(), *wp->product());
      return true;
    }

    if (auto wp =
          dynamic_cast<art::Wrapper<typename P::partner_t> const*>(product)) {
      storeassns(ds, h_e, a_map, pd.inputTag(), *wp->product());
      return true;
    }
    return false;
  }

} // namespace

namespace icaruswf {
  class LoadbalancingOutput;
}

class LoadbalancingOutput : public OutputModule {
public:
  struct Config {
    fhicl::TableFragment<OutputModule::Config> omConfig;
    fhicl::Sequence<std::string> dropCommands{fhicl::Name("dropCommands"), {}};
    fhicl::Atom<bool> async{fhicl::Name("async"), true};
    fhicl::Atom<bool> batch{fhicl::Name("batch"), true};
  };

  using Parameters =
    fhicl::WrappedTable<Config, OutputModule::Config::KeysToIgnore>;

  explicit LoadbalancingOutput(Parameters const&);

  std::map<art::ProductID, hepnos::ProductID>
  update_map_hevent(hepnos::Event h_e, std::string current_processname)
  {
    std::map<art::ProductID, hepnos::ProductID> translator;

    std::vector<hepnos::ProductID> prodIds;
    {
      std::function<void(void)> f = [&]() {
        prodIds = std::move(h_e.listProducts(""));
        return;
      };
      this->run_hepnos_func(f);
    }

    hepnos::UUID datasetId;
    hepnos::RunNumber run;
    hepnos::SubRunNumber subrun;
    hepnos::EventNumber hepnosevent;
    std::string hepnostag;
    std::string type;
    // for data products in hepnos event
    for (auto p : prodIds) {
      p.unpackInformation(
        &datasetId, &run, &subrun, &hepnosevent, &hepnostag, &type);
      auto [label, instance, processname] = splitTag(hepnostag);
      auto t = art::uniform_type_name(type);
      auto const product_name =
        art::canonicalProductName(art::friendlyname::friendlyName(t),
                                  label,
                                  instance,
                                  current_processname);
      auto art_pid = art::ProductID{product_name};
      if (translator.find(art_pid) != translator.cend())
        continue;
      translator[art_pid] = p;
      //   std::cout << "hepnos event: " << art_pid << ", " << product_name <<
      //   ", " << processname << "\n";
    }
    return translator;
  }

  std::map<art::ProductID, hepnos::ProductID>
  update_map_aevent(EventPrincipal& a_e,
                    hepnos::Event h_e,
                    hepnos::WriteBatch& batch,
                    bool wantForwardProducts,
                    std::vector<std::regex> const& dropCommands)
  {
    hepnos::StoreStatistics stats;
    std::map<art::ProductID, hepnos::ProductID> translator;
    for (auto const& pr : a_e) {
      auto const& g = *pr.second;
      auto const& pd = g.productDescription();
      // “ART_2011a”, HepnosSource
      // std::cout << "art event: " << pd.productID() << ", " << pd.branchName()
      // << ", " << pd.processName()<< "\n"; std::cout << "product Tag: " <<
      // pd.inputTag() << ", pd.present= " << pd.present() << ", forward: " <<
      // wantForwardProducts << std::endl;
      if (any_of(
            begin(dropCommands), end(dropCommands), [&pd](auto const& cmd) {
              return regex_search(pd.branchName(), cmd);
            }))
        continue;
      if (pd.present() && !wantForwardProducts)
        continue;
      auto const& oh = a_e.getForOutput(pd.productID(), true);
      // dynamic cast to the type we care about is needed here
      EDProduct const* product = oh.isValid() ? oh.wrapper() : nullptr;
      if (auto pwt = prodWithType<std::vector<raw::RawDigit>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For wires as output of signal processing and input for hit finding
      if (auto pwt = prodWithType<std::vector<recob::Wire>>(product, pd)) {
        auto begin = std::chrono::high_resolution_clock::now();
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = end - begin;
        auto ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
        std::cout << "Time for recob::Wire: " << ms << std::endl;
      }
      // For hits as output of hit finding
      // Hits are also output of Pandora?
      if (auto pwt = prodWithType<std::vector<recob::Hit>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For SpacePoints as output of Pandora and MCstage1
      if (auto pwt =
            prodWithType<std::vector<recob::SpacePoint>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For Edge as output of Pandora
      if (auto pwt = prodWithType<std::vector<recob::Edge>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For PFParticles as output of Pandora and MCStage1
      if (auto pwt =
            prodWithType<std::vector<recob::PFParticle>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For Seeds as output of Pandora
      if (auto pwt = prodWithType<std::vector<recob::Seed>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For Slices as output of Pandora
      if (auto pwt = prodWithType<std::vector<recob::Slice>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For Vertices as output of Pandora
      if (auto pwt = prodWithType<std::vector<recob::Vertex>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For Clusters as output of Pandora
      if (auto pwt = prodWithType<std::vector<recob::Cluster>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For PCAxis as output of Pandora and MCstage1
      if (auto pwt = prodWithType<std::vector<recob::PCAxis>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
      // For OpDetWaveform as output of
      if (auto pwt =
            prodWithType<std::vector<raw::OpDetWaveform>>(product, pd)) {
        std::function<void(void)> f = [&]() {
          translator[pd.productID()] =
            h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          return;
        };
        this->run_hepnos_func(f);
      }
    }

    spdlog::info("store times: num={}, avg={}, max={}, min={}",
                 stats.raw_storage_time.num,
                 stats.raw_storage_time.avg,
                 stats.raw_storage_time.max,
                 stats.raw_storage_time.min);

    spdlog::info("serialization times: num={}, avg={}, max={}, min={}",
                 stats.serialization_time.num,
                 stats.serialization_time.avg,
                 stats.serialization_time.max,
                 stats.serialization_time.min);

    return translator;
  }

  void
  store_aassns(art::EventPrincipal& p,
               hepnos::DataStore ds,
               hepnos::Event h_e,
               std::map<art::ProductID, hepnos::ProductID>& translator,
               std::vector<std::regex> const& dropCommands)
  {
    // for associaation collections in art event
    for (auto const& pr : p) {
      auto const& g = *pr.second;
      auto const& pd = g.productDescription();
      // std::cout << "art event assns: " << pd.productID() << ", " <<
      // pd.branchName() << ", " << pd.processName()<< "\n";
      if (translator.find(pd.productID()) != translator.cend())
        continue;
      if (any_of(
            begin(dropCommands), end(dropCommands), [&pd](auto const& cmd) {
              return regex_search(pd.branchName(), cmd);
            }))
        continue;
      auto const& oh = p.getForOutput(pd.productID(), true);

      // dynamic cast to the type we care about is needed here
      EDProduct const* product = oh.isValid() ? oh.wrapper() : nullptr;
      if (assnsWithType<raw::RawDigit, recob::Wire>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<raw::RawDigit, recob::Hit>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::Wire, recob::Hit>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::Seed, recob::Hit>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::Cluster, recob::Hit>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::Hit, recob::SpacePoint>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::Hit, recob::Slice>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::Edge, recob::PFParticle>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::Edge, recob::SpacePoint>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::PFParticle, recob::Seed>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::PFParticle, recob::Slice>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::PFParticle, recob::Vertex>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::PFParticle, recob::SpacePoint>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::PFParticle, recob::Cluster>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
      if (assnsWithType<recob::PCAxis, recob::PFParticle>(
            product, pd, ds, h_e, translator)) {
        continue;
      }
    }
  }

private:
  void write(EventPrincipal& e) override;
  void
  writeRun(RunPrincipal& r) override
  {}
  void
  writeSubRun(SubRunPrincipal& sr) override
  {}
  void beginJob() override;
  void endJob() override;
  void beginRun(RunPrincipal const& r) override;
  void beginSubRun(SubRunPrincipal const& sr) override;
  void respondToOpenInputFile(FileBlock const&) override;
  void run_hepnos_func(std::function<void(void)>);
  bool wantForwardProducts_;
  hepnos::Run r_;
  hepnos::SubRun sr_;
  hepnos::DataStore& datastore_;
  hepnos::AsyncEngine async_;
  hepnos::WriteBatch batch_;
  hepnos::DataSet dataset_;
  hepnos::Queue queue_;
  std::map<art::ProductID, hepnos::ProductID> translator_;
  std::vector<std::regex> dropCommands_;
}; // LoadbalancingOutput

LoadbalancingOutput::LoadbalancingOutput(Parameters const& ps)
  : OutputModule{ps().omConfig, ps.get_PSet()}
  , datastore_{art::ServiceHandle<icaruswf::HepnosDataStore>()->getStore()}
{
  auto cmds = ps().dropCommands();
  dropCommands_.reserve(cmds.size());
  for (auto cmd : cmds) {
    dropCommands_.emplace_back(cmd);
  }

  {
    std::function<void(void)> f = [&]() {
      async_ = hepnos::AsyncEngine(datastore_, 1);
      return;
    };
    this->run_hepnos_func(f);
  }

  {
    std::function<void(void)> f = [&]() {
      batch_ = hepnos::WriteBatch(async_, 2048);
      return;
    };
    this->run_hepnos_func(f);
  }

  {
    std::function<void(void)> f = [&]() {
      dataset_ = datastore_.root().createDataSet(ps().omConfig().fileName());
      return;
    };
    this->run_hepnos_func(f);
  }
}

void
LoadbalancingOutput::run_hepnos_func(std::function<void(void)> work)
{
  art::ServiceHandle<icaruswf::HepnosDataStore>()->set_work_function(work);
  art::ServiceHandle<icaruswf::HepnosDataStore>()->set_work_state();
  art::ServiceHandle<icaruswf::HepnosDataStore>()->wait();
  return;
}

void
LoadbalancingOutput::beginJob()
{
  {
    std::function<void(void)> f = [&]() {
      queue_ =
        datastore_.openQueue<EID>(art::Globals::instance()->processName(),
                                  hepnos::QueueAccessMode::PRODUCER);
      return;
    };
    this->run_hepnos_func(f);
  }
}

void
LoadbalancingOutput::endJob()
{
  /* flush the batch and close the queue */
  {
    std::function<void(void)> f = [&]() {
      batch_.flush();
      queue_.close();
      return;
    };
    this->run_hepnos_func(f);
  }

  /* reset all hepnos objects, for they may contain
     stray shared_ptrs to datastore making destruction
     tricky */
  {
    std::function<void(void)> f = [&]() {
      r_ = hepnos::Run{};
      sr_ = hepnos::SubRun{};
      async_ = hepnos::AsyncEngine{};
      batch_ = hepnos::WriteBatch{};
      dataset_ = hepnos::DataSet{};
      queue_ = hepnos::Queue{};

      return;
    };
    this->run_hepnos_func(f);
  }

  // art::ServiceHandle<icaruswf::HepnosDataStore>()->finalize();
}
void
LoadbalancingOutput::beginRun(RunPrincipal const& r)
{
  std::function<void(void)> f = [&]() {
    r_ = dataset_.createRun(r.run());
    return;
  };
  this->run_hepnos_func(f);
}

void
LoadbalancingOutput::beginSubRun(SubRunPrincipal const& sr)
{
  std::function<void(void)> f = [&]() {
    sr_ = r_.createSubRun(sr.subRun());
    return;
  };
  this->run_hepnos_func(f);
}

void
LoadbalancingOutput::respondToOpenInputFile(FileBlock const& fb)
{
  wantForwardProducts_ = fb.fileFormatVersion().era_ != "HepnosSource";
}

void
LoadbalancingOutput::write(EventPrincipal& p)
{
  if (!p.size())
    return;

  hepnos::Event h_e;
  {
    std::function<void(void)> f = [&]() {
      h_e = sr_.createEvent(p.event());
      return;
    };
    this->run_hepnos_func(f);
  }

  auto begin = std::chrono::high_resolution_clock::now();
  auto map1 =
    this->update_map_hevent(h_e, art::Globals::instance()->processName());
  translator_.insert(map1.begin(), map1.end());
  auto end = std::chrono::high_resolution_clock::now();
  auto dur = end - begin;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
  std::cout << "hepnos event products time in ms: " << ms << std::endl;

  begin = std::chrono::high_resolution_clock::now();
  auto map2 = this->update_map_aevent(
    p, h_e, batch_, wantForwardProducts_, dropCommands_);
  translator_.insert(map2.begin(), map2.end());
  end = std::chrono::high_resolution_clock::now();
  dur = end - begin;
  ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
  std::cout << "art event products time in ms: " << ms << std::endl;

  begin = std::chrono::high_resolution_clock::now();
  this->store_aassns(p, datastore_, h_e, translator_, dropCommands_);
  end = std::chrono::high_resolution_clock::now();
  dur = end - begin;
  ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
  std::cout << "art assns time in ms: " << ms << std::endl;

  EID eid{r_.number(), sr_.number(), h_e.number()};
  {
    std::function<void(void)> f = [&]() {
      queue_.push(eid);
      return;
    };
    this->run_hepnos_func(f);
  }
}

DEFINE_ART_MODULE(LoadbalancingOutput)
