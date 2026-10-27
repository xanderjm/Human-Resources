#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>
#include <human_resources/progression/account_session.h>

#include "progression_test_backend.h"
#include "test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace {
namespace gp = human_resources::gameplay;
namespace pr = human_resources::progression;
namespace st = human_resources::storage;
using test_support::require;
using progression_tests::MemoryBackend;

pr::RunReceipt receipt(const pr::AccountSession& session, const gp::Game& game) {
  const auto& stats = game.snapshot().stats;
  return {.run_id = session.account().checkpoint.run_id,
      .active_milliseconds = static_cast<std::uint64_t>(stats.active_seconds * 1000),
      .score = stats.score, .captures = stats.captures, .soldier_kills = stats.soldier_kills,
      .jeep_kills = stats.jeep_kills, .tank_kills = stats.tank_kills,
      .vehicle_captures = stats.vehicle_captures, .boss_kills = stats.boss_kills};
}
void start(pr::AccountSession& session, gp::Game& game, const std::uint8_t location = 2,
           const unsigned seed = 9317) {
  require(session.load().playable(), "load native account");
  game.start({.seed = seed, .permanent_ranks = session.account().ranks,
      .unlocked_blueprints = session.account().unlocked_blueprints});
  require(session.begin_run(pr::continuation_for(game, location)).succeeded(), "begin atomically stores initial actual Game and location");
}
void tick(gp::Game& game) {
  if (game.snapshot().phase == gp::Phase::choosing_upgrade)
    require(game.choose_upgrade(0), "actual earned choice");
  else game.advance(0.25F);
}
void play_to(gp::Game& game, const double seconds) {
  for (unsigned frame = 0; frame < 10000 && game.snapshot().stats.active_seconds < seconds; ++frame) {
    require(game.snapshot().phase != gp::Phase::dead, "early checkpoint fixture remains alive");
    tick(game);
  }
  require(game.snapshot().stats.active_seconds >= seconds, "actual native time reaches checkpoint");
}
void cold_resume_does_not_bank_and_end_is_terminal() {
  MemoryBackend backend; pr::AccountSession original(backend); gp::Game game;
  start(original, game); play_to(game, 31);
  const auto checkpoint = receipt(original, game);
  require(original.checkpoint(checkpoint, pr::continuation_for(game, 2), true).succeeded(), "background saves full actual state");
  const auto state = original.account().continuation.game_state;
  pr::AccountSession resumed(backend);
  require(resumed.load().issue == pr::LoadIssue::resumable_run && resumed.account().credits == 0 &&
      resumed.account().counters.completed_runs == 0 && resumed.account().continuation.location == 2,
      "cold active run retains identity, credits, and Desert instead of settling");
  gp::Game restored;
  require(restored.restore_state(resumed.account().continuation.game_state) == gp::SaveStateIssue::none &&
      restored.save_state() == state, "full authoritative state restores exactly");
  restored.pause(); const auto time = restored.snapshot().stats.active_seconds;
  restored.advance(1000);
  require(restored.snapshot().stats.active_seconds == time, "restored pause excludes off-process wall time");
  require(resumed.settle(receipt(resumed, restored)).succeeded(), "explicit End and Bank settles resumed run");
  const auto balance = resumed.account().credits;
  require(resumed.account().continuation.game_state.empty() && balance == pr::base_credits(checkpoint),
      "settlement clears continuation in same account commit");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::loaded && again.account().credits == balance &&
      again.account().checkpoint.run_id == 0 && again.account().continuation.game_state.empty(),
      "settled run cannot reappear as active after restart");
  require(again.settle(checkpoint).issue == pr::MutationIssue::duplicate, "same resumed receipt banks once");
}
void choice_reroll_and_pick_are_durable_boundaries() {
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game, 1);
  for (unsigned frame = 0; frame < 2000 && game.snapshot().phase != gp::Phase::choosing_upgrade; ++frame) game.advance(0.25F);
  require(game.snapshot().phase == gp::Phase::choosing_upgrade, "actual first draft earned");
  require(session.checkpoint(receipt(session, game), pr::continuation_for(game, 1), true).succeeded(), "draft entry committed before display");
  pr::AccountSession reopened(backend); require(reopened.load().issue == pr::LoadIssue::resumable_run, "pending draft resumes");
  gp::Game draft; require(draft.restore_state(reopened.account().continuation.game_state) == gp::SaveStateIssue::none,
      "draft identities restore without RNG redraw");
  require(draft.snapshot().phase == gp::Phase::choosing_upgrade && draft.snapshot().choices[0].description == game.snapshot().choices[0].description,
      "same earned options and gain text after termination");
  require(draft.reroll(), "one actual reroll");
  require(reopened.checkpoint(receipt(reopened, draft), pr::continuation_for(draft, 1), true).succeeded(), "reroll and new RNG committed together");
  const auto rerolled = draft.save_state();
  pr::AccountSession third(backend); (void)third.load(); gp::Game after;
  require(after.restore_state(third.account().continuation.game_state) == gp::SaveStateIssue::none && after.save_state() == rerolled &&
      after.snapshot().rerolls_remaining == 1, "restart does not refund reroll or regenerate offers");
  require(after.choose_upgrade(0) && third.checkpoint(receipt(third, after), pr::continuation_for(after, 1), true).succeeded(),
      "accepted pick and surplus XP are durable together");
  pr::AccountSession fourth(backend); (void)fourth.load();
  require(gp::inspect_saved_state(fourth.account().continuation.game_state).stats.level == after.snapshot().stats.level,
      "pick state survives without granting an extra level");
}
void failed_full_checkpoint_retains_exact_bytes() {
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game);
  play_to(game, 10); require(session.checkpoint(receipt(session, game), pr::continuation_for(game, 2), true).succeeded(), "old checkpoint");
  play_to(game, 21); const auto exact = pr::continuation_for(game, 2);
  backend.read_failure = st::ReadIssue::io_failure;
  require(!session.checkpoint(receipt(session, game), exact, true).succeeded() && session.pending(), "retain failed full continuation");
  game.pause(); backend.read_failure = st::ReadIssue::none;
  require(session.retry_save().succeeded() && session.account().continuation.game_state == exact.game_state &&
      session.account().counters.completed_runs == 0, "protected reload retries exact same full state without settlement");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::resumable_run && again.account().continuation.game_state == exact.game_state,
      "process death after retry preserves exact latest successful state");
  auto wrong = receipt(again, game); ++wrong.captures;
  require(again.checkpoint(wrong, exact, true).issue == pr::MutationIssue::invalid_receipt,
      "receipt and gameplay bytes cannot diverge");
  auto other_location = exact; other_location.location = 1;
  require(again.checkpoint(receipt(again, game), other_location, true).issue == pr::MutationIssue::invalid_receipt,
      "active run location cannot be silently changed");
  require(again.checkpoint(receipt(again, game), exact, true).succeeded() && !again.pending(),
      "rejected mismatched data never poisons the latest valid receipt or continuation");
}
void full_settlement_supersedes_failed_checkpoint_without_losing_authority() {
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game);
  play_to(game, 10);
  require(session.checkpoint(receipt(session, game), pr::continuation_for(game, 2), true).succeeded(), "last durable full checkpoint");
  play_to(game, 31); const auto full = receipt(session, game); const auto state = pr::continuation_for(game, 2);
  auto invented = full; ++invented.score;
  require(session.settle(invented).issue == pr::MutationIssue::invalid_receipt,
      "a full run cannot bank counters absent from its actual retained Game state");
  backend.read_failure = st::ReadIssue::io_failure;
  require(!session.checkpoint(full, state, true).succeeded(), "retain newer full checkpoint after read protection");
  require(!session.settle(full).succeeded() && session.pending_operation() == pr::SessionOperation::settle,
      "intentional End and Bank supersedes failed checkpoint while retaining full gameplay authority");
  backend.read_failure = st::ReadIssue::none;
  require(session.retry_save().succeeded() && session.account().results.receipt.active_milliseconds == full.active_milliseconds &&
      session.account().credits == pr::base_credits(full) && session.account().continuation.game_state.empty(),
      "retry banks exact final run rather than old ten-second approximation");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::loaded && again.account().counters.completed_runs == 1,
      "full settlement continuation is terminal and exact-once");
}
void dead_checkpoint_cannot_resume_or_revive() {
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game);
  for (unsigned frame = 0; frame < 20000 && game.snapshot().phase != gp::Phase::dead; ++frame) tick(game);
  require(game.snapshot().phase == gp::Phase::dead, "real native run naturally dies");
  const auto dead = receipt(session, game);
  require(session.checkpoint(dead, pr::continuation_for(game, 2), true).succeeded(), "death phase is persisted before interruption");
  pr::AccountSession reopened(backend);
  require(reopened.load().issue == pr::LoadIssue::recovered_checkpoint &&
      reopened.account().results.receipt.run_id == dead.run_id && reopened.account().continuation.game_state.empty(),
      "cold dead checkpoint banks to results rather than restoring live hull");
  require(reopened.account().credits == pr::base_credits(dead) && !reopened.store().revive_available(dead),
      "dead run preserves exact earned reward and cannot revive through restart");
}
// Fixed public-pilot style0 policy used by the existing opening qualification.
// This fixture exercises revive persistence, so controls avoid predicted hazards
// and seek reachable specimens instead of requiring a passive build to survive.
namespace revive_pilot {
float length(gp::Vec2 a) { return std::hypot(a.x, a.z); }
gp::Vec2 delta(gp::Vec2 a, gp::Vec2 b) { return {a.x-b.x,a.z-b.z}; }
gp::Vec2 plus(gp::Vec2 a, gp::Vec2 b) { return {a.x+b.x,a.z+b.z}; }
gp::Vec2 times(gp::Vec2 a,float n) { return {a.x*n,a.z*n}; }
float dot(gp::Vec2 a,gp::Vec2 b) { return a.x*b.x+a.z*b.z; }
float body(gp::EntityKind k) { return gp::tuning::military_contact_radius[static_cast<unsigned>(k)-4]; }
float ray_distance(gp::Vec2 p,gp::Vec2 a,gp::Vec2 b) {
  const auto line=delta(b,a); const float t=std::clamp(dot(delta(p,a),line)/std::max(.001F,dot(line,line)),0.F,1.F);
  return length(delta(p,plus(a,times(line,t))));
}
struct Pilot {
  gp::Vec2 previous{};
  gp::Control choose(const gp::Snapshot& s) {
    const gp::EntityView* food=nullptr; float food_score=1.e9F;
    for(const auto& e:s.entities) if(e.kind<gp::EntityKind::soldier) {
      float d=length(delta(e.position,s.player));
      // Choose a reachable pocket, not a specimen already carried off the rear edge.
      if(e.position.z-s.runner_distance < -13 || std::abs(e.position.x)>6.8F) continue;
      float threat=0;
      for(const auto& a:s.entities) if(a.kind>=gp::EntityKind::soldier)
        threat+=std::max(0.F,body(a.kind)+2.8F-length(delta(e.position,a.position)))*3;
      if(d+threat<food_score) {food_score=d+threat;food=&e;}
    }
    gp::Vec2 goal{0,s.runner_distance-2};
    if(food) goal=food->position;
    // Practical field discipline: keep room to dodge on either side and retreat.
    goal.x=std::clamp(goal.x,-6.F,6.F);
    goal.z=std::clamp(goal.z,s.runner_distance-11.F,s.runner_distance+10.F);
    const std::array<gp::Vec2,9> moves{{{0,0},{1,0},{-.7071068F,.7071068F},{0,1},{.7071068F,.7071068F},{-1,0},{-.7071068F,-.7071068F},{0,-1},{.7071068F,-.7071068F}}};
    gp::Vec2 best_move{};float best=-1.e9F,current_risk=0;
    for(const auto m:moves) {
      const gp::Vec2 velocity{m.x*s.movement_speed,m.z*s.movement_speed+s.forward_speed};
      const auto future=plus(s.player,times(velocity,.5F));
      const float relative=future.z-(s.runner_distance+s.forward_speed*.5F);
      float risk=0;
      risk=static_cast<float>(static_cast<double>(risk)+std::pow(std::max(0.F,std::abs(future.x)-6.5F),2)*25);
      risk=static_cast<float>(static_cast<double>(risk)+std::pow(std::max(0.F,-12.F-relative),2)*15+std::pow(std::max(0.F,relative-12.F),2)*15);
      for(const auto& e:s.entities) if(e.kind>=gp::EntityKind::soldier) {
        // A lifted ground body cannot contact the ship; do not avoid its stale root.
        if(e.capture_progress>0)continue;
        const auto relative_v=delta(e.velocity,velocity),offset=delta(e.position,s.player);
        const float t=std::clamp(-dot(offset,relative_v)/std::max(.001F,dot(relative_v,relative_v)),0.F,.65F);
        const float clearance=length(plus(offset,times(relative_v,t)))-body(e.kind)-gp::tuning::player_radius;
        risk=static_cast<float>(static_cast<double>(risk)+std::pow(std::max(0.F,1.F-clearance),2)*55);
        if(e.warning_seconds>0) {
          const float d=ray_distance(future,e.position,e.warning_aim);
          risk=static_cast<float>(static_cast<double>(risk)+std::pow(std::max(0.F,1.2F-d),2)*7/(.4F+e.warning_seconds));
        }
      }
      for(const auto& p:s.projectiles) {
        const auto rv=delta(p.velocity,velocity),offset=delta(p.position,s.player);
        const float t=std::clamp(-dot(offset,rv)/std::max(.001F,dot(rv,rv)),0.F,.8F);
        const float clearance=length(plus(offset,times(rv,t)))-gp::tuning::player_radius-p.radius;
        risk=static_cast<float>(static_cast<double>(risk)+std::pow(std::max(0.F,.45F-clearance),2)*35/(.2F+t));
      }
      const auto direction_goal=delta(goal,s.player);const float distance_goal=std::max(1.F,length(direction_goal));
      float score=dot(m,direction_goal)/distance_goal*3-risk;
      score+=dot(m,previous)*.45F; // retain a chosen dodge side rather than chatter each tick
      if(m.x==0&&m.z==0)current_risk=risk;
      if(score>best){best=score;best_move=m;}
    }
    previous=best_move;
    bool dash=s.dash_cooldown_seconds<=0&&current_risk>25&&length(best_move)>.5F;
    // A two metre dash must finish inside the field and outside contact bodies.
    if(dash) {
      const float distance=s.modules[static_cast<unsigned>(gp::Module::phase_drive)]?3.F:2.F;
      const auto destination=plus(s.player,times(best_move,distance));
      if(std::abs(destination.x)>7.1F||destination.z-s.runner_distance < -14.F||destination.z-s.runner_distance>14.F)dash=false;
      for(const auto& e:s.entities)if(e.kind>=gp::EntityKind::soldier&&e.capture_progress==0&&length(delta(destination,e.position))<body(e.kind)+1.1F)dash=false;
    }
    return {best_move,dash};
  }
};
} // namespace revive_pilot
void completed_revive_commits_the_actual_live_continuation() {
  // Earn eligibility with the fixed risk-aware movement policy, then release
  // controls after 91 seconds and let actual combat end the run.
  MemoryBackend backend; pr::AccountSession session(backend); gp::Game game; start(session, game, 2, 1);
  revive_pilot::Pilot pilot; gp::Control control{}; unsigned control_tick{};
  for (unsigned frame = 0; frame < 60 * 900 && game.snapshot().phase != gp::Phase::dead; ++frame) {
    if (game.snapshot().phase == gp::Phase::choosing_upgrade) {
      // This is a revive-transaction fixture, not an idle-build difficulty
      // expectation. Prefer real offered defenses while earning eligibility.
      unsigned selected=0;float best=-1;
      for(unsigned i=0;i<3;++i) {
        const auto& choice=game.snapshot().choices[i];float score=1;
        if(choice.hardware) {
          if(choice.module==gp::Module::repair_swarm)score=100;
          else if(choice.module==gp::Module::shield_generator)score=90;
          else if(choice.module==gp::Module::phase_drive)score=60;
          else if(choice.module==gp::Module::laser_array)score=30;
        } else if(choice.research==gp::Research::emergency_repairs)score=game.snapshot().hull<game.snapshot().maximum_hull*.7F?110.F:20.F;
        else if(choice.research==gp::Research::hull_reinforcement||choice.research==gp::Research::shield_capacity)score=80;
        if(score>best){best=score;selected=i;}
      }
      std::cout<<"revive fixture earned choice active="<<game.snapshot().stats.active_seconds<<" title="<<game.snapshot().choices[selected].title<<'\n';
      require(game.choose_upgrade(selected), "actual earned defensive choice in revive fixture");
      control = {}; control_tick = 0;
      continue;
    }
    if (game.snapshot().stats.active_seconds < 91) {
      if (control_tick % 6 == 0) control = pilot.choose(game.snapshot());
    } else control = {};
    game.advance(1.F / 60, control);
    control.dash = false; ++control_tick;
  }
  if(!(game.snapshot().phase==gp::Phase::dead && game.revive_available()))std::cerr<<"revive fixture phase="<<static_cast<unsigned>(game.snapshot().phase)<<" active="<<game.snapshot().stats.active_seconds<<" hull="<<game.snapshot().hull<<'\n';
  require(game.snapshot().phase == gp::Phase::dead && game.revive_available(), "eligible real death fixture");
  const auto dead = receipt(session, game);
  gp::Game candidate;
  require(candidate.restore_state(game.save_state()) == gp::SaveStateIssue::none && candidate.revive(),
      "prepare actual revived Game in an isolated candidate");
  const auto revived = pr::continuation_for(candidate, 2);
  backend.write_failure = st::WriteIssue::quota;
  require(!session.revive(dead, revived, pr::PlacementOutcome::completed).succeeded() &&
      session.pending_operation() == pr::SessionOperation::revive && !session.account().revive_used,
      "failed revived state commit retains outcome without publishing a consumed-only reward");
  backend.write_failure = st::WriteIssue::none;
  require(session.retry_save().succeeded() && session.account().revive_used, "retry atomically commits revived authority and consumption");
  pr::AccountSession cold(backend);
  require(cold.load().issue == pr::LoadIssue::resumable_run, "termination before publishing live candidate still restores earned revive");
  gp::Game recovered;
  require(recovered.restore_state(cold.account().continuation.game_state) == gp::SaveStateIssue::none &&
      recovered.snapshot().phase != gp::Phase::dead && recovered.snapshot().hull > 0 && recovered.snapshot().shield == 0 &&
      gp::inspect_saved_state(cold.account().continuation.game_state).revive_count == 1,
      "durable revived hull, zero shield and one consumed revive survive restart together");
  require(cold.revive(dead, revived, pr::PlacementOutcome::completed).issue == pr::MutationIssue::ineligible,
      "duplicate completed revive cannot grant another life");
}
std::vector<std::byte> legacy_payload(const pr::Account& account) {
  require(account.continuation.game_state.empty(), "legacy fixture cannot contain new Game bytes");
  auto bytes = pr::encode_account(account);
  require(bytes.size() >= 40, "current account prefix exists");
  bytes.resize(bytes.size() - 40); // Five schema2 tail values; schema1 prefix stays byte-exact.
  bytes[8] = std::byte{1};
  return bytes;
}
void legacy_migration_and_future_protection() {
  MemoryBackend source; pr::AccountSession old(source); (void)old.load(); (void)old.begin_run();
  const pr::RunReceipt run{.run_id = old.account().checkpoint.run_id, .active_milliseconds = 90000, .score = 500};
  require(old.checkpoint(run, true).succeeded(), "legacy receipt fixture");
  MemoryBackend backend; backend.files["account.bin"] = st::encode_envelope(1, legacy_payload(old.account()));
  const auto original = backend.files["account.bin"];
  backend.write_failure = st::WriteIssue::quota;
  pr::AccountSession migrated(backend);
  require(!migrated.load().playable() && backend.files["account.bin"] == original, "failed migration preserves original schema1 bytes");
  backend.write_failure = st::WriteIssue::none;
  require(migrated.retry_save().succeeded() && migrated.account().credits == 111 && migrated.account().results.recovered &&
      st::decode_envelope(backend.files["account.bin"], 2, 65536).valid(), "legacy checkpoint settles once and atomically migrates to schema2");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::loaded && again.account().credits == 111, "migration never duplicates settlement");
  backend.files["account.bin"] = st::encode_envelope(3, {});
  pr::AccountSession future(backend);
  require(future.load().issue == pr::LoadIssue::unsupported_schema && !future.begin_run().succeeded(), "future envelope stays protected");
}
class RecoveryFailureBackend final : public st::Backend {
public:
  MemoryBackend memory;
  bool fail_document{}, fail_backup{};
  st::ReadResult read(st::Scope scope, const st::RelativePath& path, std::size_t limit) override { return memory.read(scope, path, limit); }
  st::WriteResult write_atomically(st::Scope scope, const st::RelativePath& path, std::span<const std::byte> bytes) override {
    if ((fail_document && path.value == "account.bin") || (fail_backup && path.value != "account.bin"))
      return {.issue = st::WriteIssue::io_failure};
    return memory.write_atomically(scope, path, bytes);
  }
};
void incompatible_run_backs_up_before_recorded_settlement(unsigned old_revision) {
  RecoveryFailureBackend backend; pr::AccountSession seed_account(backend); gp::Game game;
  require(seed_account.load().playable(), "initial account exists for owned-power fixture");
  auto owned = seed_account.account();
  owned.credits = 137; owned.ranks = {1, 2, 3, 4}; owned.unlocked_blueprints = 0xffffU;
  backend.memory.files["account.bin"] = st::encode_envelope(2, pr::encode_account(owned));
  pr::AccountSession original(backend); // Cold session reads the seeded account; no trusted-cache bypass.
  start(original, game);
  play_to(game, 31); require(original.checkpoint(receipt(original, game), pr::continuation_for(game, 2), true).succeeded(), "compatible fixture");
  auto account = original.account();
  if (old_revision) {
    // A former Game authority header remains protected even inside the current
    // account envelope. It cannot be reinterpreted with newly added actor fields.
    account.continuation.game_state[8] = static_cast<std::byte>(old_revision);
    for (std::size_t i = 9; i < 12; ++i) account.continuation.game_state[i] = std::byte{};
    account.continuation.game_state[4] = static_cast<std::byte>(old_revision >= 10 ? 6 : old_revision == 9 ? 2 : 1);
    for (std::size_t i = 5; i < 8; ++i) account.continuation.game_state[i] = std::byte{};
    if (old_revision == 9) {
      constexpr std::uint64_t game9_fingerprint = 13450486980726334161ULL;
      for (std::size_t i = 0; i < 8; ++i)
        account.continuation.game_state[12 + i] = static_cast<std::byte>(game9_fingerprint >> (i * 8));
    }
    // Deliberately absent actor body and invalid checksum: reject the real
    // Game7/8/9 header pairs before interpreting the new infantry semantics.
    account.continuation.game_state.resize(28);
  } else
    account.continuation.game_state[12] ^= std::byte{1}; // Known Game header's numeric-rule fingerprint.
  const auto recorded = account.continuation.earned_base_credits;
  backend.memory.files["account.bin"] = st::encode_envelope(2, pr::encode_account(account));
  const auto incompatible = backend.memory.files["account.bin"];
  pr::AccountSession blocked(backend);
  require(blocked.load().issue == pr::LoadIssue::incompatible_continuation && !blocked.writable() &&
      !blocked.begin_run().succeeded(), "known account with incompatible Game remains protected");
  require(blocked.account().credits == owned.credits && blocked.account().ranks == owned.ranks &&
      blocked.account().unlocked_blueprints == owned.unlocked_blueprints &&
      backend.memory.files["account.bin"] == incompatible,
      "incompatible Game never erases the protected wallet, permanent ranks or unlocks");
  backend.fail_backup = true;
  require(!blocked.preserve_and_bank_checkpoint({"incompatible-backup.bin"}).succeeded() && blocked.pending() &&
      !backend.memory.files.contains("incompatible-backup.bin") && backend.memory.files["account.bin"] == incompatible &&
      blocked.account().credits == owned.credits,
      "failed backup cannot replace the original or bank historical rewards");
  backend.fail_backup = false;
  backend.fail_document = true;
  require(!blocked.retry_save().succeeded() && blocked.pending() &&
      backend.memory.files["incompatible-backup.bin"] == incompatible && backend.memory.files["account.bin"] == incompatible,
      "backup verifies before a failed replacement and preserves both exact originals");
  backend.fail_document = false;
  pr::AccountSession after_process_death(backend);
  require(after_process_death.load().issue == pr::LoadIssue::incompatible_continuation &&
      after_process_death.preserve_and_bank_checkpoint({"incompatible-backup.bin"}).succeeded(),
      "existing identical verified backup permits retry after process death");
  require(after_process_death.account().credits == owned.credits + recorded && after_process_death.account().ranks == owned.ranks &&
      after_process_death.account().unlocked_blueprints == owned.unlocked_blueprints && after_process_death.account().results.recovered &&
      after_process_death.account().continuation.game_state.empty(), "fallback banks stored historical reward without resuming incompatible rules");
  pr::AccountSession again(backend);
  require(again.load().issue == pr::LoadIssue::loaded && again.account().credits == owned.credits + recorded &&
      again.account().ranks == owned.ranks && again.account().counters.completed_runs == 1 &&
      backend.memory.files["incompatible-backup.bin"] == incompatible,
      "fallback cannot duplicate settlement on another launch");
}
void historical_results_do_not_recompute_current_reward() {
  MemoryBackend backend; pr::AccountSession session(backend); (void)session.load(); (void)session.begin_run();
  const pr::RunReceipt run{.run_id = session.account().checkpoint.run_id, .active_milliseconds = 90000, .score = 500};
  require(session.settle(run).succeeded(), "historical fixture");
  auto historical = session.account();
  historical.results.base_credits = 120; historical.results.bonus_credits = 60; historical.results.bonus_consumed = true;
  historical.counters.base_credits = 120; historical.credits = 180;
  auto bytes = pr::encode_account(historical);
  require(!bytes.empty(), "bounded recorded reward can differ from today's formula");
  backend.files["account.bin"] = st::encode_envelope(2, bytes);
  pr::AccountSession reopened(backend);
  require(reopened.load().playable() && reopened.account().credits == 180 && reopened.account().results.base_credits == 120,
      "historical reward and completed bonus remain exact after tuning changes");
  historical.results.bonus_credits = 59;
  require(pr::encode_account(historical).empty(), "contradictory policy1 bonus still rejected");
}
void maximum_continuation_fits_account_envelope() {
  MemoryBackend backend; pr::AccountSession session(backend);
  require(session.load().playable(), "fresh account supplies canonical starter unlocks");
  auto account = session.account();
  account.checkpoint.run_id = 1; account.next_run_id = 2;
  // This is a capacity fixture, not a valid Game. The Game codec test separately
  // fills every legal actor/projectile slot with a complete boss plan. Testing
  // the entire allowed 48KiB continuation is a stronger envelope size bound.
  account.continuation.game_state.resize(pr::maximum_continuation_bytes);
  const auto encoded = pr::encode_account(account);
  require(!encoded.empty() && encoded.size() <= 64 * 1024, "maximum continuation fits the unchanged account payload cap");
  const auto envelope = st::encode_envelope(2, encoded);
  require(envelope.size() <= 64 * 1024 && st::decode_envelope(envelope, 2, 64 * 1024).valid(),
      "full capacity continuation also fits and validates the protected storage envelope");
  pr::Account decoded;
  require(pr::decode_account(encoded, decoded) && decoded.continuation.game_state == account.continuation.game_state,
      "maximum bounded continuation survives account fieldwise encoding exactly");
  account.continuation.game_state.push_back(std::byte{});
  require(pr::encode_account(account).empty(), "account rejects a continuation one byte over the existing limit");
}
void malformed_current_payload_is_not_a_future_schema() {
  MemoryBackend source; pr::AccountSession original(source); (void)original.load();
  const auto payload = pr::encode_account(original.account());
  for (std::size_t size = 0; size < payload.size(); ++size) {
    MemoryBackend backend;
    backend.files["account.bin"] = st::encode_envelope(2, std::span(payload).first(size));
    pr::AccountSession session(backend);
    require(session.load().issue == pr::LoadIssue::corrupt && !session.writable(),
        "every truncated current account is corrupt rather than silently fresh or future");
  }
}
}
int main() {
  cold_resume_does_not_bank_and_end_is_terminal(); choice_reroll_and_pick_are_durable_boundaries();
  failed_full_checkpoint_retains_exact_bytes(); dead_checkpoint_cannot_resume_or_revive();
  full_settlement_supersedes_failed_checkpoint_without_losing_authority();
  legacy_migration_and_future_protection();
  incompatible_run_backs_up_before_recorded_settlement(0);
  incompatible_run_backs_up_before_recorded_settlement(7);
  incompatible_run_backs_up_before_recorded_settlement(8);
  incompatible_run_backs_up_before_recorded_settlement(9);
  incompatible_run_backs_up_before_recorded_settlement(18);
  completed_revive_commits_the_actual_live_continuation();
  historical_results_do_not_recompute_current_reward();
  maximum_continuation_fits_account_envelope();
  malformed_current_payload_is_not_a_future_schema();
}
