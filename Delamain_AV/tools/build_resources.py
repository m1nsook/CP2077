"""Compile and round-trip check the authored WolvenKit JSON; no game files needed."""
import json, os, shutil, subprocess, sys, tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cli = Path(sys.argv[1]).resolve()

def call(*args):
    subprocess.run([str(cli), *map(str, args)], check=True)

def check_cr2w(p):
    assert p.is_file() and p.read_bytes()[:4] == b"CR2W", f"Missing/invalid CR2W: {p}"

def value(x):
    return x["$value"]

with tempfile.TemporaryDirectory() as temp:
    tmp = Path(temp)
    cooked = tmp / "DelamainAV"
    dest = cooked / "m1nsook" / "delamain_av"
    dest.mkdir(parents=True)
    for source in sorted((root / "resources").glob("*.json")):
        intermediate = tmp / source.name
        shutil.copyfile(source, intermediate)
        call("convert", "deserialize", intermediate)
        result = intermediate.with_suffix("")
        check_cr2w(result)
        final = dest / result.name
        shutil.move(result, final)
        # CLI may return success on individual conversion failure. Check output and round-trip.
        call("convert", "serialize", final)
        exported = final.with_name(final.name + ".json")
        assert exported.is_file(), f"Missing round-trip export for {final}"
        data = json.loads(exported.read_text(encoding="utf-8-sig"))["Data"]["RootChunk"]
        if final.suffix == ".workspot":
            tree = data["workspotTree"]["Data"]
            nodes = [x["Data"] for x in tree["rootEntry"]["Data"]["list"]]
            assert [n["$type"] for n in nodes] == ["workEntryAnim", "workSequence", "workExitAnim"]
            assert value(nodes[0]["slotName"]) == "default"
            assert value(nodes[0]["animName"]) == "stand__2h_on_sides__01__to__sit_couch__AV_excalibur__01__turn270__getting_into_AV__01"
            assert value(nodes[2]["animName"]) == "sit_couch__AV_excalibur__01__to__stand__2h_on_sides__01__turn270__01"
            assert nodes[2]["id"]["id"] == 6
            assert nodes[2]["flags"] == 8
            assert value(nodes[2]["slotName"]) == "default"
            assert nodes[2]["stayOnNavmesh"] == 0
            assert nodes[2]["snapZToNavmesh"] == 0
            assert nodes[2]["forceNoZSnap"] == 1
            assert nodes[1]["loopInfinitely"] == 1
            assert len(tree["finalAnimsets"]) == 2
            # The original q203_04_delamain.scene maps the disembark clip to
            # cinematicAnimNames[1] / cinematicAnimSets[1], not q113.
            exit_animset = r"base\animations\quest\main_quests\prologue\q000\q000_corpo_03b_fly_to_lizzies\body\q000_corpo_03b_fly_to_lizzies__player_male.anims"
            for rig in tree["finalAnimsets"]:
                matches = [entry for entry in rig["animations"]["cinematics"]
                           if value(entry["animSet"]["DepotPath"]) == exit_animset]
                assert len(matches) == 1 and matches[0]["priority"] == 128
                assert sum(value(h["DepotPath"]) == exit_animset for h in rig["loadingHandles"]) == 1
            print("q000 disembark animset verified for both player rigs", flush=True)
        else:
            compiled = data["compiledData"]["Data"]["Chunks"]
            assert data["entity"]["Data"]["$type"] == "vehicleCarBaseObject"
            assert compiled[0]["$type"] == "vehicleCarBaseObject"
            by_name = {value(c["name"]): c for c in compiled if "name" in c}
            physics_rig = by_name["vehicle_rig"]
            deformation = by_name["deformation_rig"]
            assert physics_rig["id"] == "1651752295586041860"
            assert value(physics_rig["rig"]["DepotPath"]) == r"base\vehicles\base_rig\v_base_x4_150_149_27\v_base_x4_150_149_27.rig"
            assert value(physics_rig["graph"]["DepotPath"]) == r"base\vehicles\base_rig\v_base_x4_150_149_27\v_base_x4_150_149_27.animgraph"
            assert value(physics_rig["controlBinding"]["Data"]["bindName"]) == "AnimationController"
            assert deformation["id"] == "1651752295586041856"
            assert value(deformation["controlBinding"]["Data"]["bindName"]) == "vehicle_rig"
            assert value(deformation["parentTransform"]["Data"]["bindName"]) == "vehicle_rig"
            assert value(deformation["rig"]["DepotPath"]) == r"base\vehicles\special\av_rayfield_excalibur\rig\av_rayfield_excalibur_01__basic_01.rig"
            assert all("base\\dav\\" not in json.dumps(c) for c in (physics_rig, deformation))
            print("Car physics rig and original Excalibur deformation hierarchy verified", flush=True)
            component = next(x for x in compiled if x["$type"] == "gameOccupantSlotComponent")
            assert component["id"] == "1640045072000864256"
            assert value(component["name"]) == "OccupantSlots"
            passenger = component["slotData"][0]
            assert value(passenger["slotName"]) == "seat_back_left"
            assert passenger["role"] == "Passenger"
            assert value(passenger["workSpotResource"]["DepotPath"]) == r"m1nsook\delamain_av\passenger.workspot"
        exported.unlink()
        print(f"CR2W round-trip verified: {final.name}", flush=True)
    assert sorted(p.name for p in dest.iterdir()) == ["occupant_patch.ent", "passenger.workspot"]
    archive_dir = root / "archive" / "pc" / "mod"
    call("pack", cooked, "-o", archive_dir)
    archive = archive_dir / "DelamainAV.archive"
    assert archive.is_file() and archive.read_bytes()[:4] == b"RDAR", "Archive missing or invalid"
    print(f"Native archive built: {archive.stat().st_size} bytes", flush=True)
