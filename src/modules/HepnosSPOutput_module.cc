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
#include "fhiclcpp/types/ConfigurationTable.h"
#include "canvas/Persistency/Provenance/canonicalProductName.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <regex>
#include <vector>

#include "hepnos.hpp"

#include "../serialization/cluster_serialization.h"
#include "../serialization/edge_serialization.h"
#include "../serialization/hit_serialization.h"
#include "../serialization/opdetwaveform_serialization.h"
#include "../serialization/opflash_serialization.h"
#include "../serialization/ophit_serialization.h"
#include "../serialization/hit_serialization.h"
#include "../serialization/pcaxis_serialization.h"
#include "../serialization/pfparticle_serialization.h"
#include "../serialization/rawdigit_serialization.h"
#include "../serialization/seed_serialization.h"
#include "../serialization/slice_serialization.h"
#include "../serialization/spacepoint_serialization.h"
#include "../serialization/vertex_serialization.h"
#include "../serialization/wire_serialization.h"

#include <boost/serialization/utility.hpp>
#include "HepnosDataStore.h"
#include "canvas/Utilities/uniform_type_name.h"

#include "canvas/Utilities/FriendlyName.h"

using namespace art;
namespace hepnos {
  template <typename A, typename B>
    using Assns =std::vector<std::pair<hepnos::Ptr<A>, hepnos::Ptr<B>>>;
  template <typename A, typename B, typename D>
    using AssnsD =std::vector<std::tuple<hepnos::Ptr<A>, hepnos::Ptr<B>, D>>;
}

namespace {

  std::tuple<std::string, std::string, std::string> splitTag(std::string const& inputtag) {
    //InputTag: label = 'daq0', instance = 'PHYSCRATEDATATPCEE', process = 'DetSim'
    //For clarity I am defining all these variables
    art::InputTag a_inputtag;
    decode(inputtag, a_inputtag);

    std::string label = a_inputtag.label();
    std::string instance = a_inputtag.instance();
    std::string process = a_inputtag.process();
    return std::make_tuple(label, instance, process);
  }

  template <typename P>
    P const * prodWithType(art::EDProduct const* product, art::BranchDescription const& pd) {
      if (product == nullptr) return nullptr;
      if (pd.producedClassName() != art::TypeID{typeid(P)}.className()) return nullptr;
      if (auto wp = dynamic_cast<art::Wrapper<P> const*>(product))
        return wp->product();
      return nullptr;
    }


  template<typename A, typename B>
    void
    storeassns(hepnos::DataStore datastore,
        hepnos::Event & h_e,
        std::map<art::ProductID, hepnos::ProductID> const& a_map,
        art::InputTag const& a_t,
        art::Assns<A, B> const& a_assns
        )
    {
      hepnos::Assns<A, B> h_assns;
      h_assns.reserve(a_assns.size());

      for (auto const & [a,b]: a_assns) {
      //  std::cout <<"storeassns, art prod keys: " << a.key() << ", " << b.key() << std::endl;
      //  std::cout << "storeassns, art prod IDs: " << a.id() << ", " << b.id() << std::endl;
        auto const A_ptr = datastore.makePtr<A>(a_map.at(a.id()), a.key());
        auto const B_ptr = datastore.makePtr<B>(a_map.at(b.id()), b.key());
        h_assns.emplace_back(A_ptr, B_ptr);
      }
     h_e.store(a_t.encode(), h_assns);
     // std::cout << "Assns in art event: " << a_t.process() << ", " << a_t.label() << ", " << a_t.encode() << "\n";
    }

  template<typename A, typename B, typename D>
    void
    storeassns(hepnos::DataStore datastore,
        hepnos::Event & h_e,
        std::map<art::ProductID, hepnos::ProductID> const& a_map,
        art::InputTag const& a_t,
        art::Assns<A, B, D> const& a_assns
        )
    {
      hepnos::AssnsD<A, B, D> h_assns;
      h_assns.reserve(a_assns.size());
      for (auto const & [a,b, d]: a_assns) {
        auto const A_ptr = datastore.makePtr<A>(a_map.at(a.id()), a.key());
        auto const B_ptr = datastore.makePtr<B>(a_map.at(b.id()), b.key());
        h_assns.emplace_back(A_ptr, B_ptr, d);
      }
      h_e.store(a_t, h_assns);
      //std::cout << "Assns in art event: " << a_t.process() << ", " << a_t.label() << ", " << a_t.encode() << "\n";
    }

  template <typename A, typename B, typename D=void>
    bool assnsWithType(art::EDProduct const* product, 
                       art::BranchDescription const& pd,
                       hepnos::DataStore ds,
                       hepnos::Event & h_e,
                       std::map<art::ProductID, hepnos::ProductID> const& a_map) {
      using P = art::Assns<A, B, D>;
      if (product == nullptr) return false;
      if (pd.friendlyClassName() != art::TypeID{typeid(P)}.friendlyClassName()) return false;
      if (auto wp = dynamic_cast<art::Wrapper<P> const*>(product)) {
        storeassns(ds, h_e, a_map, pd.inputTag(), *wp->product());
        return true;
        }
      
      if (auto wp = dynamic_cast<art::Wrapper<typename P::partner_t> const*>(product)) {
        storeassns(ds, h_e, a_map, pd.inputTag(), *wp->product());
        return true;
        }
        return false;
    }
  
  std::map<art::ProductID, hepnos::ProductID>
    update_map_hevent(hepnos::Event h_e, std::string current_processname)
    {
      std::map<art::ProductID, hepnos::ProductID> translator;
      auto prodIds = h_e.listProducts("");
      hepnos::UUID datasetId;
      hepnos::RunNumber run;
      hepnos::SubRunNumber subrun;
      hepnos::EventNumber hepnosevent;
      std::string hepnostag;
      std::string type;
      //for data products in hepnos event
      for (auto p: prodIds) {
        p.unpackInformation(&datasetId, &run, &subrun, &hepnosevent, &hepnostag, &type);
        auto [label, instance, processname] = splitTag(hepnostag);
        auto t = art::uniform_type_name(type);
        auto const product_name = art::canonicalProductName(art::friendlyname::friendlyName(t),
            label,
            instance,
            current_processname);
        auto art_pid = art::ProductID{product_name};
        if (translator.find(art_pid) != translator.cend()) continue;
        translator[art_pid] = p;
     //   std::cout << "hepnos event: " << art_pid << ", " << product_name << ", " << processname << "\n";
      }
      return translator;
    }

  std::map<art::ProductID, hepnos::ProductID>
    update_map_aevent(EventPrincipal& a_e, hepnos::Event h_e, hepnos::WriteBatch &batch, bool wantForwardProducts, std::vector<std::regex> const& dropCommands) {
      hepnos::StoreStatistics stats;
      std::map<art::ProductID, hepnos::ProductID> translator;
      for (auto const& pr : a_e) {
        auto const& g = *pr.second;
        auto const& pd = g.productDescription();
        //“ART_2011a”, HepnosSource
        //std::cout << "art event: " << pd.productID() << ", " << pd.branchName() << ", " << pd.processName()<< "\n";
        //std::cout << "product Tag: " << pd.inputTag() << ", pd.present= " << pd.present() << ", forward: " << wantForwardProducts << std::endl;
        if (any_of(begin(dropCommands), end(dropCommands), [&pd](auto const& cmd){return regex_search(pd.branchName(), cmd);})) continue; 
        if (pd.present() && !wantForwardProducts) continue;
        auto const& oh = a_e.getForOutput(pd.productID(), true);
        //dynamic cast to the type we care about is needed here
        EDProduct const* product = oh.isValid() ? oh.wrapper() : nullptr;
        if (auto pwt = prodWithType<std::vector<raw::RawDigit>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        // For wires as output of signal processing and input for hit finding
        if (auto pwt = prodWithType<std::vector<recob::Wire>>(product, pd)) {
          auto begin = std::chrono::high_resolution_clock::now();
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
          auto end = std::chrono::high_resolution_clock::now();
          auto dur = end - begin;
          auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
          std::cout << "Time for recob::Wire: " << ms << std::endl;
        }
        // For hits as output of hit finding
        // Hits are also output of Pandora?
        if (auto pwt = prodWithType<std::vector<recob::Hit>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        // For SpacePoints as output of Pandora and MCstage1
        if (auto pwt = prodWithType<std::vector<recob::SpacePoint>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        // For Edge as output of Pandora
        if (auto pwt = prodWithType<std::vector<recob::Edge>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        // For PFParticles as output of Pandora and MCStage1
        if (auto pwt = prodWithType<std::vector<recob::PFParticle>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        // For Seeds as output of Pandora
        if (auto pwt = prodWithType<std::vector<recob::Seed>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        // For Slices as output of Pandora
        if (auto pwt = prodWithType<std::vector<recob::Slice>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        // For Vertices as output of Pandora
        if (auto pwt = prodWithType<std::vector<recob::Vertex>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        // For Clusters as output of Pandora
        if (auto pwt = prodWithType<std::vector<recob::Cluster>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        // For PCAxis as output of Pandora and MCstage1
        if (auto pwt = prodWithType<std::vector<recob::PCAxis>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
        }
        //For OpDetWaveform as output of
        if (auto pwt = prodWithType<std::vector<raw::OpDetWaveform>>(product, pd)) {
          translator[pd.productID()] = h_e.store(batch, pd.inputTag().encode(), *pwt, &stats);
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
        std::vector<std::regex> const& dropCommands) {
      // for associaation collections in art event
      for (auto const& pr : p) {
        auto const& g = *pr.second;
        auto const& pd = g.productDescription();
        //std::cout << "art event assns: " << pd.productID() << ", " << pd.branchName() << ", " << pd.processName()<< "\n";
        if (translator.find(pd.productID()) != translator.cend()) continue;
        if (any_of(begin(dropCommands), end(dropCommands), [&pd](auto const& cmd){return regex_search(pd.branchName(), cmd);})) continue; 
        auto const& oh = p.getForOutput(pd.productID(), true);

        //dynamic cast to the type we care about is needed here
        EDProduct const* product = oh.isValid() ? oh.wrapper() : nullptr;
        if (assnsWithType<raw::RawDigit, recob::Wire>(product, pd, ds, h_e, translator)) {
          continue;
          }
        if (assnsWithType<raw::RawDigit, recob::Hit>(product, pd, ds, h_e, translator)) {
          continue;
          }
        if (assnsWithType<recob::Wire,recob::Hit>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::Seed,recob::Hit>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::Cluster, recob::Hit>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::Hit,recob::SpacePoint>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::Hit,recob::Slice>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::Edge,recob::PFParticle>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::Edge,recob::SpacePoint>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::PFParticle,recob::Seed>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::PFParticle,recob::Slice>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::PFParticle,recob::Vertex>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::PFParticle,recob::SpacePoint>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::PFParticle, recob::Cluster>(product, pd, ds, h_e, translator)) {
          continue;
        }
        if (assnsWithType<recob::PCAxis, recob::PFParticle>(product, pd, ds, h_e, translator)) {
          continue;
        }
      }
    }
} // namespace

namespace icaruswf {
  class HepnosSPOutput;
}

class HepnosSPOutput : public OutputModule {
  public:
    struct Config {
      fhicl::TableFragment<OutputModule::Config> omConfig;
      fhicl::Sequence<std::string> dropCommands{fhicl::Name("dropCommands"), {}};
    };

    using Parameters =
      fhicl::WrappedTable<Config, OutputModule::Config::KeysToIgnore>;

    explicit HepnosSPOutput(Parameters const&);

  private:
    void write(EventPrincipal& e) override;
    void writeRun(RunPrincipal& r) override {}
    void writeSubRun(SubRunPrincipal& sr) override{}
    void beginRun(RunPrincipal const& r) override;
    void beginSubRun(SubRunPrincipal const& sr) override;
    void respondToOpenInputFile(FileBlock const&) override;
    bool wantForwardProducts_;
    hepnos::Run r_;
    hepnos::SubRun sr_;
    hepnos::DataStore & datastore_;
    hepnos::AsyncEngine async_;
    hepnos::WriteBatch batch_;
    hepnos::DataSet dataset_;
    std::map<art::ProductID, hepnos::ProductID> translator_;
    std::vector<std::regex> dropCommands_;
}; // HepnosSPOutput

HepnosSPOutput::HepnosSPOutput(Parameters const& ps)
  : OutputModule{ps().omConfig, ps.get_PSet()}
  , datastore_{art::ServiceHandle<icaruswf::HepnosDataStore>()->getStore()}
  , async_{datastore_, 8}
  , batch_{async_, 2048}
  , dataset_{datastore_.root().createDataSet(ps().omConfig().fileName())}
{
  auto cmds = ps().dropCommands();
  dropCommands_.reserve(cmds.size());
  for (auto cmd: cmds) {
    dropCommands_.emplace_back(cmd);
  }
}


  void
HepnosSPOutput::beginRun(RunPrincipal const& r)
{
  r_ = dataset_.createRun(r.run());
}

  void
HepnosSPOutput::beginSubRun(SubRunPrincipal const& sr)
{
  sr_ = r_.createSubRun(sr.subRun());
}

void 
HepnosSPOutput::respondToOpenInputFile(FileBlock const& fb)
{
  wantForwardProducts_ = fb.fileFormatVersion().era_ != "HepnosSource"; 
}

  void
HepnosSPOutput::write(EventPrincipal& p)
{
  if (!p.size())
    return;
  hepnos::Event h_e = sr_.createEvent(p.event());

  auto begin = std::chrono::high_resolution_clock::now();
  auto map1 = update_map_hevent(h_e, art::Globals::instance()->processName());
  translator_.insert(map1.begin(), map1.end());
  auto end = std::chrono::high_resolution_clock::now();
  auto dur = end - begin;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
  std::cout << "hepnos event products time in ms: " << ms << std::endl;

  begin = std::chrono::high_resolution_clock::now();
  auto map2 = update_map_aevent(p, h_e, batch_, wantForwardProducts_, dropCommands_);
  translator_.insert(map2.begin(), map2.end());
  end = std::chrono::high_resolution_clock::now();
  dur = end - begin;
  ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
  std::cout << "art event products time in ms: " << ms << std::endl;

  begin = std::chrono::high_resolution_clock::now();
  store_aassns(p, datastore_, h_e, translator_, dropCommands_);
  end = std::chrono::high_resolution_clock::now();
  dur = end - begin;
  ms = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
  std::cout << "art assns time in ms: " << ms << std::endl;
}


DEFINE_ART_MODULE(HepnosSPOutput)
