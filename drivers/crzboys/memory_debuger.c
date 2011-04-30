#include <linux/module.h> 
#include <linux/err.h> 
#include <linux/platform_device.h> 
#include <linux/types.h> 
#include <asm/io.h> 
struct memory_debugger_data {
	int id;
};

int str2hex(char* buf, char** dest, int n)
{
        int i, buffer;
        char temp;

        buffer = 0;
        for(i=0;i<n;i++)
        {
                temp = *(buf + i);

                if(temp >= 'a' && temp <= 'f') {
                        buffer = buffer << 4;
                        buffer = buffer | (temp - 'a' + 10);
                } else if(temp >= '0' && temp <= '9') {
                        buffer = buffer << 4;
                        buffer = buffer | (temp - '0');
                }
        }

        if(dest)
                *dest = buf + i;

        return buffer;
}

static ssize_t memory_debugger_read_reg(struct device *dev, struct device_attribute *attr, char *buf, size_t count)
{
	uint32_t reg;

	reg = str2hex(buf, NULL, 8);
	printk(KERN_ERR "read : %x -> %x\n", reg, readl(reg));
	return strnlen(buf, PAGE_SIZE);
}
static DEVICE_ATTR(read_reg, S_IWUSR, NULL, memory_debugger_read_reg);

static ssize_t memory_debugger_write_reg(struct device *dev, struct device_attribute *attr, char *buf, size_t count)
{
	uint32_t reg, val;
	char *temp;

	reg = str2hex(buf, &temp, 8);
	val = str2hex(temp, NULL, 8);

	__raw_writel(reg, val);
	printk(KERN_ERR "write : %x %x\n", reg, __raw_readl(reg));

	return strnlen(buf, PAGE_SIZE);
}
static DEVICE_ATTR(write_reg, S_IWUSR, NULL, memory_debugger_write_reg);

static struct attribute *memory_debugger_sysfs_entries[] = {
        &dev_attr_read_reg.attr,
        &dev_attr_write_reg.attr,
        NULL
};

static struct attribute_group memory_debugger_attr_group = {
        .attrs = memory_debugger_sysfs_entries,
};

static int memory_debugger_probe(struct platform_device *pdev) 
{ 
	int ret; 
	struct memory_debugger_data *data; 

	data = kzalloc(sizeof(*data), GFP_KERNEL); 
	if (data == NULL) { 
		ret = -ENOMEM; 
		goto err_data_alloc_failed; 
	}

	platform_set_drvdata(pdev, data); 

	ret = sysfs_create_group(&pdev->dev.kobj, &memory_debugger_attr_group);
	if (ret)
		goto err_sys_create_failed;

	printk(KERN_INFO "%s: probed\n", __func__);

	return 0; 
  
err_sys_create_failed: 
	kfree(data); 
err_data_alloc_failed: 
	return ret; 
} 
  
static int memory_debugger_remove(struct platform_device *pdev) 
{
	struct memory_debugger_data *data = platform_get_drvdata(pdev); 

	kfree(data); 

	return 0; 
} 
 
static struct platform_driver memory_debugger_device = { 
	.probe		 = memory_debugger_probe, 
	.remove		 = memory_debugger_remove, 
	.driver = { 
		.name = "memory-debugger" 
	}
}; 
 
static int __init memory_debugger_init(void) 
{ 
	return platform_driver_register(&memory_debugger_device); 
} 
 
static void __exit memory_debugger_exit(void) 
{ 
	platform_driver_unregister(&memory_debugger_device); 
} 
  
module_init(memory_debugger_init); 
module_exit(memory_debugger_exit); 
 
MODULE_AUTHOR("Pyeong Jeong Lee leepjung@crz-tech.com"); 
MODULE_LICENSE("GPL"); 
MODULE_DESCRIPTION("Memory Debugger driver for system"); 

