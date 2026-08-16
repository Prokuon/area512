MRuby::Gem::Specification.new('picoruby-area512-micropython') do |gem_specification|
  gem_specification.license = 'MIT'
  gem_specification.author = 'hamachan'
  gem_specification.summary = 'Run Python source files with MicroPython'

  gem_specification.cc.include_paths <<
    "#{gem_specification.dir}/../../../area512_micropython/include"
end
