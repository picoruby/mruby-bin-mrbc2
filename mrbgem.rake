MRuby::Gem::Specification.new 'mruby-bin-mrbc' do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby & PicoRuby developers'
  spec.summary = 'mruby compiler executable'
  spec.add_dependency "mruby-compiler2"
  spec.add_conflict 'mruby-compiler'

  cc.defines << %w(MRC_TARGET_PICORUBY)

  compiler2_dir = build.gems['mruby-compiler2'].dir

  cc.include_paths << "#{compiler2_dir}/lib/prism/include"

  if cc.defines.flatten.include? "MRC_PARSER_LRAMA"
    exe_name = 'mrbc_lrama'
  else
    exe_name = 'mrbc_prism'
  end

  exec = exefile("#{build.build_dir}/bin/#{exe_name}")
  mrbc2_objs = Dir.glob("#{dir}/tools/mrbc/*.c").map { |f| objfile(f.pathmap("#{build_dir}/tools/mrbc/%n")) }

  mrbc2_objs << build.libmruby_static

  if cc.defines.flatten.include? "MRC_PARSER_LRAMA"
    cc.defines << "UNIVERSAL_PARSER"
    libruby_parser_dir = "#{compiler2_dir}/lib/libruby-parser"
    cc.include_paths << "#{libruby_parser_dir}/include"
    cc.include_paths << "#{libruby_parser_dir}/lib/ruby"
    cc.include_paths << "#{libruby_parser_dir}/lib/ruby/include"
    cc.include_paths << "#{libruby_parser_dir}/lib/ruby/.ext/include/x86_64-linux"
    mrbc2_objs << "#{libruby_parser_dir}/lib/ruby/libruby-static.a"
    libraries = ["z", "crypt", "gmp"]
  else
    libraries = []
  end

  file exec => mrbc2_objs do |t|
    build.linker.run t.name, t.prerequisites, libraries
  end

  build.bins << exe_name

end
