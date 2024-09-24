MRuby::Gem::Specification.new 'mruby-bin-mrbc2' do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby & PicoRuby developers'
  spec.summary = 'mruby compiler executable'
  spec.add_dependency "mruby-compiler2"
  if cc.defines.include?('MRC_CUSTOM_ALLOC')
    spec.add_dependency 'picoruby-mrubyc' # For alloc.c
  end
  spec.add_conflict 'mruby-compiler'
  spec.add_conflict 'mruby-bin-picorbc'

  cc.defines << %w(MRC_TARGET_PICORUBY)

  compiler2_dir = build.gems['mruby-compiler2'].dir

  cc.include_paths << "#{compiler2_dir}/lib/prism/include"

  exe_name = 'picorbc'

  exec = exefile("#{build.build_dir}/bin/#{exe_name}")

  mrbc2_objs = Dir.glob("#{dir}/tools/mrbc/*.c").map { |f| objfile(f.pathmap("#{build_dir}/tools/mrbc/%n")) }
  mrbc2_objs += build.gems['mruby-compiler2'].objs
  mrbc2_objs.delete_if{|o| o.include?("gem_init")}

  libraries = []

  # Order of linking objects matters
  mrbc2_objs << build.libmruby_static
  unless cc.defines.include?('MRC_CUSTOM_ALLOC')
    mrbc2_objs.delete_if{|o| o.include?("libmruby")}
  end

  file exec => mrbc2_objs do |t|
    build.linker.run t.name, t.prerequisites, libraries
  end

  build.bins << exe_name

end
