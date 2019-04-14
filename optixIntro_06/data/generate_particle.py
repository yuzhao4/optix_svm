import numpy as np
import subprocess

W = 200
H = 200
D = 500
PART_NUM = 2000

def generate_particle(W,H,D,PART_NUM):
    particles_coords = np.zeros((PART_NUM,4))

    particles_coords[:,0] = np.random.uniform(-W/2, W/2, PART_NUM)
    particles_coords[:,1] = np.random.uniform(-H/2, H/2, PART_NUM)
    particles_coords[:,2] = np.random.uniform(-D, -300, PART_NUM)

    particles_coords[:,-1] = np.random.normal(1,0.1,PART_NUM)

    return particles_coords

IMAGE_NUM = 2

#zerosNumMax = PART_NUM / 10

count = 0
zerosNumMax = len(str(abs(PART_NUM)))

for i in range(IMAGE_NUM):
    particles = generate_particle(W,H,D,PART_NUM)

    zeros = '0' * int(zerosNumMax - len(str(i)) - 1) 
    
    
    txtname = "/home/yu/optix_advanced_examples/bin/coordinates/" + "particles_generated" + zeros + str(i) + ".txt"   

    pngname = "/home/yu/optix_advanced_examples/bin/images/" + "particles_generated" + zeros + str(i) + ".png"
    
    np.savetxt(txtname, particles,fmt='%.3f',
               delimiter=' ',newline='\n')

    np.savetxt("/home/yu/optix_src/optix_advanced_samples-master/src/optixIntroduction/optixIntro_06/data/particles_generated.txt", particles,fmt='%.3f',
               delimiter=' ',newline='\n')


    subprocess.run(["/home/yu/optix_advanced_examples/bin/optixIntro_06", "-l","-f",pngname])
