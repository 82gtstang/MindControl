extends Node3D
# Original procedural third-person fantasy RPG prototype.
# No external models are required: meshes, trees, ruins and enemies are generated.
const WALK_SPEED := 6.0
const SPRINT_SPEED := 9.0
const WORLD_SIZE := 56
var hero: CharacterBody3D
var hero_visual: Node3D
var hero_animation: AnimationPlayer
var inventory: Array[String] = []
var world_items: Array[Node3D] = []
var inventory_panel: PanelContainer
var inventory_text: Label
var equipment_panel: PanelContainer
var equipment_text: Label
var equipped_weapon := "Fists"
var equipped_armor := "Traveler Clothes"
var selected_inventory_index := -1
var item_icons: Dictionary = {}
var inside_dungeon := false
var dungeon_root: Node3D
var dungeon_entry := Vector3(13, 0, 11)
var spin_cooldown := 0.0
const HERO_MODEL := "res://assets/characters/hero.glb"
const ENEMY_MODEL := "res://assets/characters/enemy.glb"
var camera: Camera3D
var pivot: Node3D
var sword: Node3D
var enemies: Array[Node3D] = []
var treasures: Array[Node3D] = []
var torches: Array[Node3D] = []
var coins := 0
var sigils := 0
var health := 100
var attack_timer := 0.0
var walk_phase := 0.0
var message_timer := 8.0
var message := "Recover the three sigils from the ancient ruins."
var hud: Label
var message_label: Label
var move_axis := Vector2.ZERO
var touch_move_id := -1
var touch_start := Vector2.ZERO
var look_id := -1
var look_last := Vector2.ZERO
var yaw := 0.0
var pitch := -0.20
var elapsed := 0.0
var rng := RandomNumberGenerator.new()

func _ready() -> void:
	rng.seed = 7129
	RenderingServer.set_default_clear_color(Color(0.12, 0.15, 0.2))
	_create_lighting()
	_create_ground()
	_create_nature_details()
	_create_ruins()
	_create_hero()
	_create_enemies()
	_create_dungeon()
	_scatter_loot()
	_create_ui()
	Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)

func material(color: Color, rough: float = 0.9, emission: Color = Color.BLACK) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = rough
	if emission != Color.BLACK:
		m.emission_enabled = true
		m.emission = emission
		m.emission_energy_multiplier = 2.0
	return m

func box_mesh(parent: Node3D, size: Vector3, pos: Vector3, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = size
	mi.mesh = mesh
	mi.position = pos
	mi.material_override = mat
	parent.add_child(mi)
	return mi

func sphere_mesh(parent: Node3D, radius: float, pos: Vector3, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.radial_segments = 24
	mesh.rings = 12
	mi.mesh = mesh
	mi.position = pos
	mi.material_override = mat
	parent.add_child(mi)
	return mi

func cylinder_mesh(parent: Node3D, radius: float, height: float, pos: Vector3, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius * 1.12
	mesh.height = height
	mesh.radial_segments = 20
	mi.mesh = mesh
	mi.position = pos
	mi.material_override = mat
	parent.add_child(mi)
	return mi

func _create_lighting() -> void:
	var environment := WorldEnvironment.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.15, 0.18, 0.23)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.43, 0.48, 0.58)
	env.ambient_light_energy = 0.65
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.fog_enabled = true
	env.fog_light_color = Color(0.16, 0.2, 0.24)
	env.fog_density = 0.014
	environment.environment = env
	add_child(environment)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-48, -35, 0)
	sun.light_color = Color(1, 0.77, 0.54)
	sun.light_energy = 0.85
	sun.shadow_enabled = true
	add_child(sun)

func _create_ground() -> void:
	var grass := material(Color(0.16, 0.29, 0.19))
	var floor_body := StaticBody3D.new()
	add_child(floor_body)
	box_mesh(floor_body, Vector3(116, 0.4, 116), Vector3(0, -0.25, 0), grass)
	var floor_shape := CollisionShape3D.new()
	var floor_box := BoxShape3D.new()
	floor_box.size = Vector3(116, 0.4, 116)
	floor_shape.shape = floor_box
	floor_shape.position = Vector3(0, -0.25, 0)
	floor_body.add_child(floor_shape)
	var stone := material(Color(0.36, 0.32, 0.29))
	var dirt := material(Color(0.30, 0.24, 0.19))
	for i in range(240):
		var x := rng.randf_range(-53, 53)
		var z := rng.randf_range(-53, 53)
		if absf(x) < 7.0 or absf(z) < 3.0:
			continue
		var patch := sphere_mesh(self, rng.randf_range(.35, 1.0), Vector3(x, .006, z), dirt)
		patch.scale = Vector3(1.5, .025, 1.0)
		patch.rotation.y = rng.randf_range(0, 6.28)
	for i in range(105):
		var x := rng.randf_range(-51, 51)
		var z := rng.randf_range(-51, 51)
		if absf(x) < 11 and absf(z) < 14:
			continue
		_make_tree(Vector3(x, 0, z), rng.randf_range(0.8, 1.5))
	for i in range(75):
		var x := rng.randf_range(-48, 48)
		var z := rng.randf_range(-48, 48)
		if absf(x) < 7 or absf(z) < 3:
			continue
		var rock := sphere_mesh(self, rng.randf_range(.25, .7), Vector3(x, .16, z), stone)
		rock.scale = Vector3(rng.randf_range(.8, 1.5), rng.randf_range(.35, .7), rng.randf_range(.8, 1.5))
		rock.rotation.y = rng.randf_range(0, 6.28)
	for i in range(-8, 9):
		var slab := cylinder_mesh(self, rng.randf_range(1.55, 1.9), .08, Vector3(rng.randf_range(-.12, .12), .035, i * 3.1), material(Color(0.35 + rng.randf_range(-.05, .05), .32, .29)))
		slab.scale.z = .86
	_make_water(Vector3(-20, 0, -14))

func _make_tree(pos: Vector3, scale_factor: float) -> void:
	var tree := Node3D.new()
	tree.position = pos
	tree.scale = Vector3.ONE * scale_factor
	add_child(tree)
	var bark := material(Color(.23 + rng.randf_range(-.04, .04), .16, .105), 1.0)
	var leaf := material(Color(rng.randf_range(.08, .16), rng.randf_range(.20, .34), rng.randf_range(.09, .16)))
	var trunk := cylinder_mesh(tree, .23, 4.2, Vector3(0, 2.1, 0), bark)
	trunk.rotation.z = rng.randf_range(-.09, .09)
	for j in range(7):
		var angle := float(j) * 2.39996
		var height := 2.0 + float(j % 4) * .58
		var branch := cylinder_mesh(tree, .085, 1.9, Vector3(cos(angle) * .65, height + .4, sin(angle) * .65), bark)
		branch.rotation.z = sin(angle) * .75
		branch.rotation.x = cos(angle) * .75
	for j in range(11):
		var angle := float(j) * 2.39996
		var level := float(j % 4)
		var canopy := sphere_mesh(tree, rng.randf_range(.85, 1.25), Vector3(cos(angle) * (1.0 + level * .13), 3.4 + level * .47, sin(angle) * (1.0 + level * .13)), leaf)
		canopy.scale = Vector3(1.12, .68, 1.05)
	var crown := sphere_mesh(tree, 1.35, Vector3(0, 5.0, 0), leaf)
	crown.scale.y = .72

func _make_water(pos: Vector3) -> void:
	var water := Node3D.new()
	water.position = pos
	water.name = "Moonpool"
	add_child(water)
	var mat := material(Color(.09, .29, .40), .16, Color(.015, .06, .09))
	box_mesh(water, Vector3(13, .05, 9), Vector3(0, .04, 0), mat)
	for i in range(20):
		var lily := sphere_mesh(water, rng.randf_range(.1, .28), Vector3(rng.randf_range(-6, 6), .1, rng.randf_range(-4, 4)), material(Color(.14, .34, .14)))
		lily.scale.y = .1

func _create_nature_details() -> void:
	# Small environmental accents give the forest a more organic silhouette.
	var grass_mats := [
		material(Color(.19, .31, .14)),
		material(Color(.28, .34, .16)),
		material(Color(.13, .25, .12))
	]
	var fern_mat := material(Color(.11, .27, .14))
	var flower_mats := [
		material(Color(.72, .62, .34)),
		material(Color(.66, .46, .60)),
		material(Color(.75, .74, .65))
	]
	# MultiMesh batches hundreds of blades into only a few draw calls.
	for variant in range(3):
		var blade_mesh := PlaneMesh.new()
		blade_mesh.size = Vector2(.095 + variant * .035, .32 + variant * .12)
		var blade_mat: StandardMaterial3D = grass_mats[variant]
		blade_mat.cull_mode = BaseMaterial3D.CULL_DISABLED
		blade_mesh.material = blade_mat
		var mm := MultiMesh.new()
		mm.transform_format = MultiMesh.TRANSFORM_3D
		mm.mesh = blade_mesh
		mm.instance_count = 800
		for i in range(mm.instance_count):
			var x := rng.randf_range(-53.0, 53.0)
			var z := rng.randf_range(-53.0, 53.0)
			if absf(x) < 3.0:
				x += 3.5 if x >= 0.0 else -3.5
			var basis := Basis(Vector3.UP, rng.randf_range(0.0, TAU))
			basis = basis.scaled(Vector3(rng.randf_range(.7, 1.3), rng.randf_range(.7, 1.5), 1.0))
			mm.set_instance_transform(i, Transform3D(basis, Vector3(x, .19, z)))
		var instance := MultiMeshInstance3D.new()
		instance.multimesh = mm
		add_child(instance)
	for i in range(70):
		var x := rng.randf_range(-49.0, 49.0)
		var z := rng.randf_range(-49.0, 49.0)
		if absf(x) < 5.0 and absf(z) < 12.0:
			continue
		var bush := Node3D.new()
		bush.position = Vector3(x, .12, z)
		add_child(bush)
		for j in range(4):
			var angle := float(j) * TAU / 4.0
			var frond := sphere_mesh(bush, rng.randf_range(.25, .42), Vector3(cos(angle) * .27, .22 + float(j % 2) * .12, sin(angle) * .27), fern_mat)
			frond.scale = Vector3(.9, .42, 1.45)
			frond.rotation.y = angle
	for i in range(45):
		var x := rng.randf_range(-49.0, 49.0)
		var z := rng.randf_range(-49.0, 49.0)
		var blossom := Node3D.new()
		blossom.position = Vector3(x, .1, z)
		add_child(blossom)
		var stem := cylinder_mesh(blossom, .018, .35, Vector3(0, .19, 0), fern_mat)
		stem.rotation.z = rng.randf_range(-.25, .25)
		sphere_mesh(blossom, .09, Vector3(0, .39, 0), flower_mats[i % flower_mats.size()])
	for i in range(35):
		var x := rng.randf_range(-50.0, 50.0)
		var z := rng.randf_range(-50.0, 50.0)
		var pebble := sphere_mesh(self, rng.randf_range(.12, .25), Vector3(x, .025, z), material(Color(.31, .30, .27)))
		pebble.scale = Vector3(1.5, .35, 1.0)

func _create_ruins() -> void:
	var stone := material(Color(.35, .35, .34))
	var dark := material(Color(.23, .23, .25))
	var gold := material(Color(.65, .43, .17), .28)
	for j in range(2):
		var center := Vector3(-27 if j == 0 else 28, 0, -30 if j == 0 else -29)
		_make_temple(center, stone, dark, gold)
	_make_temple(Vector3(30, 0, 30), stone, dark, gold)
	for i in range(10):
		var z := -27.0 + i * 6.0
		_make_torch(Vector3(-4.3, 0, z))
		_make_torch(Vector3(4.3, 0, z))
	_make_torch(Vector3(-25, 0, -27))
	_make_torch(Vector3(26, 0, -27))
	_make_torch(Vector3(28, 0, 31))

func _make_temple(pos: Vector3, stone: Material, dark: Material, gold: Material) -> void:
	var temple := Node3D.new()
	temple.position = pos
	add_child(temple)
	box_mesh(temple, Vector3(11, .8, 9), Vector3(0, .4, 0), stone)
	for x in [-4.5, 4.5]:
		for z in [-3.5, 3.5]:
			cylinder_mesh(temple, .52, 5.0, Vector3(x, 3.2, z), stone)
			box_mesh(temple, Vector3(1.5, .45, 1.5), Vector3(x, 5.8, z), dark)
	box_mesh(temple, Vector3(11.5, .7, 1.5), Vector3(0, 6.2, -3.5), dark)
	box_mesh(temple, Vector3(11.5, .7, 1.5), Vector3(0, 6.2, 3.5), dark)
	box_mesh(temple, Vector3(1.5, 3.4, 1.0), Vector3(0, 2.5, -3.5), gold)
	var chest := Node3D.new()
	chest.position = Vector3(0, 1.0, 0)
	temple.add_child(chest)
	box_mesh(chest, Vector3(1.2, .7, .8), Vector3.ZERO, material(Color(.27, .12, .06)))
	box_mesh(chest, Vector3(1.25, .16, .85), Vector3(0, .37, 0), gold)
	treasures.append(chest)

func _make_torch(pos: Vector3) -> void:
	var torch := Node3D.new()
	torch.position = pos
	add_child(torch)
	cylinder_mesh(torch, .14, 2.4, Vector3(0, 1.2, 0), material(Color(.22, .17, .12)))
	sphere_mesh(torch, .27, Vector3(0, 2.55, 0), material(Color(1, .42, .06), .3, Color(1, .29, .02)))
	var light := OmniLight3D.new()
	light.position.y = 2.7
	light.light_color = Color(1, .47, .18)
	light.light_energy = 1.8
	light.omni_range = 7.0
	torch.add_child(light)
	torches.append(torch)

func _load_character_model(path: String, parent: Node3D, target_height: float) -> Node3D:
	if not ResourceLoader.exists(path):
		return null
	var scene := load(path) as PackedScene
	if scene == null:
		push_warning("Character asset is not a valid GLB scene: " + path)
		return null
	var model := scene.instantiate() as Node3D
	if model == null:
		return null
	parent.add_child(model)
	var bounds := _model_bounds(model)
	if bounds.size.y > .01:
		model.scale = Vector3.ONE * (target_height / bounds.size.y)
		model.position.y = -bounds.position.y * model.scale.y
	return model

func _model_bounds(root: Node3D) -> AABB:
	var combined := AABB()
	var found := false
	for child in root.find_children("*", "MeshInstance3D", true, false):
		var mesh_instance := child as MeshInstance3D
		if mesh_instance.mesh == null:
			continue
		var local_box := mesh_instance.get_aabb()
		var relative := root.global_transform.affine_inverse() * mesh_instance.global_transform
		var transformed := relative * local_box
		if not found:
			combined = transformed
			found = true
		else:
			combined = combined.merge(transformed)
	return combined

func _find_animation_player(root: Node) -> AnimationPlayer:
	if root is AnimationPlayer:
		return root as AnimationPlayer
	for child in root.get_children():
		var found := _find_animation_player(child)
		if found != null:
			return found
	return null

func _play_character_animation(player: AnimationPlayer, keywords: Array[String]) -> void:
	if player == null:
		return
	for name in player.get_animation_list():
		var lower := String(name).to_lower()
		for keyword in keywords:
			if lower.contains(keyword):
				if player.current_animation != name:
					player.play(name, .15)
				return

func _create_hero() -> void:
	hero = CharacterBody3D.new()
	hero.name = "Knight"
	hero.position = Vector3(0, .08, 8)
	hero.floor_snap_length = .25
	add_child(hero)
	var capsule := CapsuleShape3D.new()
	capsule.radius = .42
	capsule.height = 1.8
	var collider := CollisionShape3D.new()
	collider.shape = capsule
	collider.position.y = 1
	hero.add_child(collider)
	pivot = Node3D.new()
	hero.add_child(pivot)
	hero_visual = _load_character_model(HERO_MODEL, pivot, 2.15)
	if hero_visual != null:
		hero_animation = _find_animation_player(hero_visual)
	else:
		_create_placeholder_hero(pivot)
	camera = Camera3D.new()
	camera.current = true
	camera.fov = 68
	add_child(camera)

func _create_placeholder_hero(parent: Node3D) -> void:
	var armor := material(Color(.19, .32, .51), .5)
	var metal := material(Color(.58, .64, .7), .25)
	var leather := material(Color(.24, .12, .08))
	var chest_plate := sphere_mesh(pivot, .65, Vector3(0, 1.3, 0), armor)
	chest_plate.scale = Vector3(.75, 1.0, .48)
	sphere_mesh(pivot, .35, Vector3(0, 2.03, 0), metal)
	var helm := sphere_mesh(pivot, .37, Vector3(0, 2.13, 0), metal)
	helm.scale.y = .55
	for x in [-.57, .57]:
		sphere_mesh(pivot, .22, Vector3(x, 1.6, 0), metal)
		var arm := cylinder_mesh(pivot, .14, .72, Vector3(x, 1.13, 0), armor)
		arm.rotation.z = x * .18
	for x in [-.25, .25]:
		cylinder_mesh(pivot, .18, .75, Vector3(x, .45, 0), leather)
	sword = Node3D.new()
	sword.position = Vector3(.78, 1.1, -.05)
	pivot.add_child(sword)
	box_mesh(sword, Vector3(.12, 1.25, .12), Vector3(0, .55, 0), metal)
	box_mesh(sword, Vector3(.55, .11, .16), Vector3(0, -.07, 0), gold_material())

func gold_material() -> Material:
	return material(Color(.78, .53, .19), .3)

func _create_enemies() -> void:
	var positions := [Vector3(-13, 0, -16), Vector3(14, 0, -17), Vector3(-17, 0, 24), Vector3(22, 0, 19), Vector3(8, 0, -34), Vector3(-34, 0, 12)]
	for pos in positions:
		var e := Node3D.new()
		e.position = pos
		add_child(e)
		var imported_enemy := _load_character_model(ENEMY_MODEL, e, 1.9)
		if imported_enemy != null:
			var enemy_anim := _find_animation_player(imported_enemy)
			_play_character_animation(enemy_anim, ["idle", "breath"])
			enemies.append(e)
			continue
		var body := material(Color(.29, .12, .14))
		var torso := sphere_mesh(e, .68, Vector3(0, 1.0, 0), body)
		torso.scale = Vector3(.72, 1.05, .55)
		sphere_mesh(e, .38, Vector3(0, 1.85, 0), material(Color(.19, .18, .19)))
		for x in [-.18, .18]:
			sphere_mesh(e, .075, Vector3(x, 1.9, -.33), material(Color(1, .18, .06), .3, Color(.8, .04, .01)))
		enemies.append(e)

func _create_ui() -> void:
	var layer := CanvasLayer.new()
	add_child(layer)
	var panel := ColorRect.new()
	panel.color = Color(0.025, .035, .06, .75)
	panel.position = Vector2(14, 12)
	panel.size = Vector2(400, 95)
	layer.add_child(panel)
	hud = Label.new()
	hud.position = Vector2(27, 22)
	hud.add_theme_font_size_override("font_size", 25)
	layer.add_child(hud)
	message_label = Label.new()
	message_label.position = Vector2(27, 67)
	message_label.add_theme_font_size_override("font_size", 18)
	layer.add_child(message_label)
	var hint := Label.new()
	hint.text = "LEFT: MOVE     RIGHT: LOOK     ATTACK / POTION"
	hint.position = Vector2(25, 665)
	hint.add_theme_font_size_override("font_size", 18)
	layer.add_child(hint)
	var attack := Button.new()
	attack.text = "ATTACK"
	attack.position = Vector2(1060, 555)
	attack.size = Vector2(175, 65)
	attack.pressed.connect(_attack)
	layer.add_child(attack)
	var potion := Button.new()
	potion.text = "HEAL"
	potion.position = Vector2(1060, 630)
	potion.size = Vector2(175, 60)
	potion.pressed.connect(_heal)
	layer.add_child(potion)
	var jump := Button.new()
	jump.text = "JUMP"
	jump.position = Vector2(870, 630)
	jump.size = Vector2(175, 60)
	jump.pressed.connect(_jump)
	layer.add_child(jump)
	var spin := Button.new()
	spin.text = "SPIN"
	spin.position = Vector2(870, 555)
	spin.size = Vector2(175, 65)
	spin.pressed.connect(_spin_attack)
	layer.add_child(spin)
	var bag := Button.new()
	bag.text = "BAG"
	bag.position = Vector2(680, 630)
	bag.size = Vector2(175, 60)
	bag.pressed.connect(_toggle_bag)
	layer.add_child(bag)
	var interact := Button.new()
	interact.text = "PICK UP / ENTER"
	interact.position = Vector2(650, 555)
	interact.size = Vector2(210, 65)
	interact.pressed.connect(_interact)
	layer.add_child(interact)
	var throw_button := Button.new()
	throw_button.text = "THROW"
	throw_button.position = Vector2(470, 555)
	throw_button.size = Vector2(165, 65)
	throw_button.pressed.connect(_throw_from_bag)
	layer.add_child(throw_button)
	inventory_panel = PanelContainer.new()
	inventory_panel.position = Vector2(420, 80)
	inventory_panel.custom_minimum_size = Vector2(440, 380)
	layer.add_child(inventory_panel)
	inventory_text = Label.new()
	inventory_panel.add_child(inventory_text)
	inventory_panel.visible = false
	_update_bag()
	var next_button := Button.new()
	next_button.text = "NEXT ITEM"
	next_button.position = Vector2(430, 470)
	next_button.size = Vector2(150, 55)
	next_button.pressed.connect(_next_inventory_item)
	layer.add_child(next_button)
	var equip_button := Button.new()
	equip_button.text = "EQUIP"
	equip_button.position = Vector2(585, 470)
	equip_button.size = Vector2(130, 55)
	equip_button.pressed.connect(_equip_selected)
	layer.add_child(equip_button)
	var equipment_button := Button.new()
	equipment_button.text = "EQUIPMENT"
	equipment_button.position = Vector2(470, 630)
	equipment_button.size = Vector2(200, 60)
	equipment_button.pressed.connect(_toggle_equipment)
	layer.add_child(equipment_button)
	equipment_panel = PanelContainer.new()
	equipment_panel.position = Vector2(740, 95)
	equipment_panel.custom_minimum_size = Vector2(320, 260)
	layer.add_child(equipment_panel)
	equipment_text = Label.new()
	equipment_panel.add_child(equipment_text)
	equipment_panel.visible = false
	_update_equipment()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		if event.pressed:
			if event.position.x < get_viewport().get_visible_rect().size.x * .45 and touch_move_id < 0:
				touch_move_id = event.index
				touch_start = event.position
			elif look_id < 0:
				look_id = event.index
				look_last = event.position
		else:
			if event.index == touch_move_id:
				touch_move_id = -1
				move_axis = Vector2.ZERO
			if event.index == look_id:
				look_id = -1
	elif event is InputEventScreenDrag:
		if event.index == touch_move_id:
			move_axis = ((event.position - touch_start) / 95.0).limit_length(1)
		elif event.index == look_id:
			_rotate_camera(event.position - look_last)
			look_last = event.position
	elif event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
		_rotate_camera(event.relative)

func _rotate_camera(delta: Vector2) -> void:
	yaw -= delta.x * .005
	pitch = clampf(pitch - delta.y * .004, -.65, .35)

func _physics_process(delta: float) -> void:
	elapsed += delta
	message_timer -= delta
	attack_timer = maxf(0, attack_timer - delta)
	spin_cooldown = maxf(0, spin_cooldown - delta)
	var axis := move_axis
	axis.x += Input.get_axis("ui_left", "ui_right")
	axis.y += Input.get_axis("ui_up", "ui_down")
	axis = axis.limit_length(1)
	var forward := Vector3(sin(yaw), 0, cos(yaw))
	var right := Vector3(cos(yaw), 0, -sin(yaw))
	var direction := right * axis.x + forward * axis.y
	if direction.length() > .1:
		hero.velocity.x = direction.x * WALK_SPEED
		hero.velocity.z = direction.z * WALK_SPEED
		pivot.rotation.y = lerp_angle(pivot.rotation.y, atan2(-direction.x, -direction.z) + PI, delta * 9)
		_play_character_animation(hero_animation, ["run", "walk", "locomotion"])
		walk_phase += delta * 11
		pivot.position.y = absf(sin(walk_phase)) * .055
	else:
		hero.velocity.x = move_toward(hero.velocity.x, 0, delta * 25)
		hero.velocity.z = move_toward(hero.velocity.z, 0, delta * 25)
		_play_character_animation(hero_animation, ["idle", "breath"])
		pivot.position.y = lerpf(pivot.position.y, 0.0, delta * 5)
	if Input.is_action_just_pressed("ui_accept"):
		_jump()
	if not hero.is_on_floor():
		hero.velocity.y -= 24 * delta
	elif hero.velocity.y < 0.0:
		hero.velocity.y = 0.0
	hero.move_and_slide()
	if inside_dungeon:
		hero.position.x = clampf(hero.position.x, -11, 11)
		hero.position.z = clampf(hero.position.z, 70, 108)
	else:
		hero.position.x = clampf(hero.position.x, -54, 54)
		hero.position.z = clampf(hero.position.z, -54, 54)
	var look_target := hero.global_position + Vector3(0, 1.65, 0)
	var desired := look_target + Vector3(sin(yaw) * 5.8, 2.8 - pitch * 4.0, cos(yaw) * 5.8)
	camera.global_position = camera.global_position.lerp(desired, minf(delta * 7, 1))
	camera.look_at(look_target, Vector3.UP)
	for i in range(torches.size()):
		var light := torches[i].get_node_or_null("OmniLight3D") as OmniLight3D
		if light:
			light.light_energy = 1.7 + sin(elapsed * 8 + i * 3) * .3
	for e in enemies:
		if not is_instance_valid(e):
			continue
		e.position.y = sin(elapsed * 2.3 + e.position.x) * .08
		if e.position.distance_to(hero.position) < 2.0 and int(elapsed * 2.0) != int((elapsed - delta) * 2.0):
			health = maxi(0, health - 4)
			message = "A shadow strikes! Defend yourself."
			message_timer = 2
	for chest in treasures.duplicate():
		if hero.global_position.distance_to(chest.global_position) < 2.1:
			treasures.erase(chest)
			chest.visible = false
			sigils += 1
			coins += 50
			message = "Ancient sigil recovered! %d / 3" % sigils
			message_timer = 5
			if sigils == 3:
				message = "Victory! You restored the three ancient sigils."
	if health <= 0:
		message = "You have fallen. Restart the game to try again."
		hero.velocity = Vector3.ZERO
	hud.text = "ASHENVALE   HP: %d   GOLD: %d   SIGILS: %d/3" % [health, coins, sigils]
	message_label.text = message if message_timer > 0 or health <= 0 else ""

func _attack() -> void:
	if attack_timer > 0:
		return
	attack_timer = .6
	if hero_animation != null:
		_play_character_animation(hero_animation, ["attack", "slash", "swing"])
	elif sword != null:
		var tween := create_tween()
		tween.tween_property(sword, "rotation:z", -1.1, .13)
		tween.tween_property(sword, "rotation:z", 0.0, .25)
	for e in enemies.duplicate():
		if hero.global_position.distance_to(e.global_position) < 3.2:
			enemies.erase(e)
			e.queue_free()
			coins += 10
			message = "Shadow defeated! +10 gold."
			message_timer = 3
			break

func _heal() -> void:
	if coins >= 20 and health < 100:
		coins -= 20
		health = mini(100, health + 40)
		message = "Restored 40 health for 20 gold."
		message_timer = 3

func _jump() -> void:
	if (hero.is_on_floor() or hero.global_position.y <= .25) and health > 0:
		hero.velocity.y = 11.0
		_play_character_animation(hero_animation, ["jump"])

func _spin_attack() -> void:
	if spin_cooldown > 0.0 or health <= 0:
		return
	spin_cooldown = 2.0
	attack_timer = .7
	var tween := create_tween()
	tween.tween_property(pivot, "rotation:y", pivot.rotation.y + TAU, .52)
	_play_character_animation(hero_animation, ["spin", "attack"])
	for e in enemies.duplicate():
		if is_instance_valid(e) and e.global_position.distance_to(hero.global_position) < 3.8:
			enemies.erase(e)
			e.queue_free()
			coins += 10
	message = "Whirlwind strike!"
	message_timer = 2.0

func _toggle_bag() -> void:
	_update_bag()
	inventory_panel.visible = not inventory_panel.visible

func _toggle_equipment() -> void:
	_update_equipment()
	equipment_panel.visible = not equipment_panel.visible

func _update_equipment() -> void:
	if equipment_text != null:
		equipment_text.text = "CHARACTER EQUIPMENT\n\nMain Hand: " + equipped_weapon + "\nArmor: " + equipped_armor + "\n\nChoose an item in your bag, then EQUIP."

func _equip_selected() -> void:
	if selected_inventory_index < 0 or selected_inventory_index >= inventory.size():
		message = "Select an item from your bag first."
		message_timer = 3.0
		return
	var item: String = inventory[selected_inventory_index]
	if item.contains("Sword") or item.contains("Dagger"):
		equipped_weapon = item
	elif item.contains("Boots") or item.contains("Shield"):
		equipped_armor = item
	else:
		message = "That item cannot be equipped."
		message_timer = 3.0
		return
	message = "Equipped " + item
	message_timer = 3.0
	_update_equipment()
	_update_bag()

func _select_inventory_item(index: int) -> void:
	selected_inventory_index = index
	_update_bag()

func _update_bag() -> void:
	if inventory_text == null:
		return
	var lines := "TRAVELER'S BAG (%d / 200)\n\n" % inventory.size()
	for i in range(mini(inventory.size(), 16)):
		var marker := "> " if i == selected_inventory_index else "  "
		lines += "%s%d. %s\n" % [marker, i + 1, inventory[i]]
	lines += "\nUse NEXT to select, EQUIP to wear or wield."
	inventory_text.text = lines

func _next_inventory_item() -> void:
	if inventory.is_empty():
		return
	selected_inventory_index = (selected_inventory_index + 1) % inventory.size()
	_update_bag()

func _interact() -> void:
	if inside_dungeon and hero.position.distance_to(Vector3(0, .2, 72)) < 4.0:
		inside_dungeon = false
		hero.position = dungeon_entry + Vector3(0, .3, 2)
		message = "You emerge from the ancient crypt."
		message_timer = 4
		return
	if not inside_dungeon and hero.position.distance_to(dungeon_entry) < 4.0:
		inside_dungeon = true
		hero.position = Vector3(0, .2, 73)
		message = "You descend into the forgotten crypt."
		message_timer = 5
		return
	for item in world_items.duplicate():
		if is_instance_valid(item) and hero.global_position.distance_to(item.global_position) < 2.6:
			if inventory.size() >= 200:
				message = "Bag is full."
				return
			inventory.append(str(item.get_meta("item_name", "Unknown relic")))
			selected_inventory_index = inventory.size() - 1
			world_items.erase(item)
			item.queue_free()
			_update_bag()
			message = "Collected " + inventory.back()
			message_timer = 3
			return
	if not inventory.is_empty():
		_throw_item()

func _throw_item() -> void:
	var name: String = inventory.pop_back()
	selected_inventory_index = mini(selected_inventory_index, inventory.size() - 1)
	var forward := -pivot.global_transform.basis.z.normalized()
	var drop := Node3D.new()
	drop.position = hero.position + forward * 2.4 + Vector3(0, .35, 0)
	drop.set_meta("item_name", name)
	add_child(drop)
	var shape := sphere_mesh(drop, .23, Vector3.ZERO, material(Color(.75, .52, .22), .4))
	shape.scale.y = .7
	world_items.append(drop)
	_update_bag()
	message = "Threw " + name
	message_timer = 3

func _scatter_loot() -> void:
	var names := ["Iron Sword", "Silver Dagger", "Health Potion", "Mana Potion", "Old Coin", "Ancient Key", "Rope", "Torch", "Crystal", "Herbs", "Leather Boots", "Iron Shield", "Rune Stone", "Emerald", "Ruby", "Map Fragment", "Mushroom", "Ring", "Arrow Bundle", "Spell Scroll"]
	var gold_mat := material(Color(.74, .52, .17), .4)
	var crystal_mat := material(Color(.22, .65, .76), .25, Color(.02, .08, .12))
	for i in range(320):
		var dungeon_item := i >= 260
		var x := rng.randf_range(-48, 48) if not dungeon_item else rng.randf_range(-8, 8)
		var z := rng.randf_range(-48, 48) if not dungeon_item else rng.randf_range(76, 105)
		if not dungeon_item and absf(x) < 3.0 and absf(z) < 10.0:
			x += 5.0
		var item := Node3D.new()
		item.name = "Loot_%d" % i
		item.position = Vector3(x, .22, z)
		var item_name: String = names[i % names.size()]
		item.set_meta("item_name", item_name)
		add_child(item)
		var mesh := sphere_mesh(item, .19, Vector3.ZERO, crystal_mat if i % 5 == 0 else gold_mat)
		mesh.scale = Vector3(.8, 1.2, .8)
		world_items.append(item)

func _create_dungeon() -> void:
	var stone := material(Color(.19, .20, .22))
	var floor_mat := material(Color(.27, .25, .24))
	var stairs_mat := material(Color(.33, .30, .26))
	# Exterior entry arch and a visible descending stairwell.
	var entrance := Node3D.new()
	entrance.position = dungeon_entry
	add_child(entrance)
	for x in [-2.0, 2.0]:
		cylinder_mesh(entrance, .46, 4.0, Vector3(x, 2.0, 0), stone)
	box_mesh(entrance, Vector3(5.3, .7, 1.5), Vector3(0, 4.2, 0), stone)
	for i in range(8):
		box_mesh(entrance, Vector3(3.0, .2, 1.1), Vector3(0, -.12 - float(i) * .18, -float(i) * .9), stairs_mat)
	dungeon_root = Node3D.new()
	dungeon_root.position = Vector3(0, 0, 70)
	add_child(dungeon_root)
	# Dungeon occupies a separate section of the world to avoid overlap with the forest.
	var body := StaticBody3D.new()
	dungeon_root.add_child(body)
	box_mesh(body, Vector3(24, .6, 42), Vector3(0, -.4, 19), floor_mat)
	var collision := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = Vector3(24, .6, 42)
	collision.shape = shape
	collision.position = Vector3(0, -.4, 19)
	body.add_child(collision)
	for x in [-12.0, 12.0]:
		box_mesh(dungeon_root, Vector3(1.0, 6.0, 42), Vector3(x, 2.7, 19), stone)
	for z in [0.0, 40.0]:
		box_mesh(dungeon_root, Vector3(24, 6.0, 1.0), Vector3(0, 2.7, z), stone)
	for i in range(7):
		var z := 5.0 + float(i) * 5.0
		for x in [-8.0, 8.0]:
			cylinder_mesh(dungeon_root, .48, 5.0, Vector3(x, 2.5, z), stone)
		_make_torch(Vector3(-9.0, 0, 70 + z))
		_make_torch(Vector3(9.0, 0, 70 + z))
	box_mesh(dungeon_root, Vector3(4, 1.0, 3), Vector3(0, .5, 34), stairs_mat)
	var portal := sphere_mesh(dungeon_root, 1.2, Vector3(0, 2.0, 35), material(Color(.12, .29, .40), .25, Color(.06, .24, .39)))
	portal.scale.z = .25

func _throw_from_bag() -> void:
	if not inventory.is_empty():
		_throw_item()
